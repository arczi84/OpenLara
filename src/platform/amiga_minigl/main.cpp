#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <SDL/SDL.h>

#include "game.h"

// Include Exec calls after OpenLara/MiniGL: the MiniGL compatibility include
// temporarily renames Amiga's generic `Node` type to avoid C++ name clashes.
#define Node AmigaExecNode
#include <proto/exec.h>
#include <proto/intuition.h>
#undef Node

#define WND_TITLE       "OpenLara MiniGL"
#define FRAME_INTERVAL_MS 20
#define SND_FREQ        44100
// Match the Amiga SDL/AHI backend's own ~46 ms default.  A 512-frame buffer
// requires an expensive OpenLara mix every ~12 ms and crackles on Coffin when
// the audio task shares priority with the renderer.
#define SND_FRAMES      2048
// Priority 0 produced audible underruns while MiniGL rendered continuously.
// Priority 1 is the smallest value that lets the mixer pre-empt the game;
// the task blocks in ahi.device between buffers, so it cannot spin at this
// priority like the original SDL value of 11 did during startup.
#define SND_TASK_PRIORITY 1
#define MAX_JOYS        4
#define JOY_DEAD_ZONE   8192

#ifndef __AMIGADATE__
#define __AMIGADATE__ "31.8.2026"
#endif

static const char versionTag[] = "$VER: " WND_TITLE " 1.6 (" __AMIGADATE__ ")";

extern "C" {
// Retained for launchers which inspect the conventional stack request.  Do
// not force libnix's __stkinit into the binary: its __stkexit teardown returns
// through a corrupted stack under the GCC 16.2 nix20 runtime.
unsigned long __stack = 1024 * 1024;
}

static SDL_Surface  *screen;
static SDL_Joystick *joysticks[MAX_JOYS];
static int joystickCount;
static vec2 joyL, joyR;
static bool disableAudio;
static FILE *startupLog;
static int displayDepth = 32;
static bool fullscreen;

struct VideoMode {
    int width;
    int height;
};

static const VideoMode videoModes[] = {
    {  320, 240 },
    {  512, 384 },
    {  640, 480 },
    {  800, 600 },
    { 1024, 768 },
    { 1280, 960 },
    {  960, 540 },
    { 1024, 576 },
    { 1280, 720 },
    { 1280, 800 },
    { 1280, 1024 },
    { 1366, 768 },
    { 1440, 900 },
    { 1600, 900 },
    { 1680, 1050 },
    { 1920, 1080 },
};
static_assert(COUNT(videoModes) == Core::Settings::AMIGA_RES_MAX, "Video modes must match saved IDs");

static int resolutionIndex = Core::Settings::AMIGA_RES_640_480;

static Sound::Frame *sndData;
static bool audioReady;
static int sndBufferFrames;
static Uint32 sndCallbackCount;
static Uint32 sndLastCallbackMS;
static Uint32 sndMaxCallbackGapMS;
static Uint32 sndMaxMixMS;
static Uint32 sndLateGapCount;
static Uint32 sndLateMixCount;



static void traceStartup(const char *text) {
    puts(text);
    if (startupLog) {
        fputs(text, startupLog);
        fputc('\n', startupLog);
        fflush(startupLog);
    }
}

int osLoadTraceFrames = 0;

void osTraceLoad(const char *format, ...) {
    if (!startupLog) return;
    char text[256];
    va_list args;
    va_start(args, format);
    vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    traceStartup(text);
}

static void traceSDLError(const char *stage) {
    const char *error = SDL_GetError();
    printf("%s: %s\n", stage, error);
    if (startupLog) {
        fprintf(startupLog, "%s: %s\n", stage, error);
        fflush(startupLog);
    }
}

static void closeStartupLog() {
    if (startupLog) {
        fclose(startupLog);
        startupLog = NULL;
    }
}

static void centerMiniGLWindow() {
    struct Window *window = (struct Window *)mglGetWindowHandle();
    if (!window || !window->WScreen) {
        traceStartup("07 window centering: MiniGL window not available");
        return;
    }

    int left = ((int)window->WScreen->Width  - (int)window->Width)  / 2;
    int top  = ((int)window->WScreen->Height - (int)window->Height) / 2;
    left = max(0, left);
    top  = max(0, top);

    MoveWindow(window, left - window->LeftEdge, top - window->TopEdge);

    char info[128];
    snprintf(info, sizeof(info),
             "07 window centered: %dx%d at %d,%d on %dx%d",
             (int)window->Width, (int)window->Height, left, top,
             (int)window->WScreen->Width, (int)window->WScreen->Height);
    traceStartup(info);
}

void* osMutexInit()                         { return SDL_CreateMutex(); }
void  osMutexFree(void *obj)                { if (obj) SDL_DestroyMutex((SDL_mutex*)obj); }
void  osMutexLock(void *obj)                { if (obj) SDL_LockMutex((SDL_mutex*)obj); }
void  osMutexUnlock(void *obj)              { if (obj) SDL_UnlockMutex((SDL_mutex*)obj); }

int osGetTimeMS() {
    return (int)SDL_GetTicks();
}

bool osJoyReady(int index) {
    return index >= 0 && index < joystickCount && joysticks[index] != NULL;
}

void osJoyVibrate(int index, float left, float right) {
    (void)index;
    (void)left;
    (void)right;
}

static void sndFill(void *userdata, Uint8 *stream, int len) {
    (void)userdata;
    Uint32 start = SDL_GetTicks();
    Uint32 expected = (sndBufferFrames * 1000U) / SND_FREQ;
    if (sndLastCallbackMS) {
        Uint32 gap = start - sndLastCallbackMS;
        sndMaxCallbackGapMS = max(sndMaxCallbackGapMS, gap);
        if (gap > expected + expected / 2)
            sndLateGapCount++;
    }
    sndLastCallbackMS = start;
    sndCallbackCount++;

    int frames = len / (int)sizeof(Sound::Frame);
    if (frames > sndBufferFrames)
        frames = sndBufferFrames;
    Sound::fill(sndData, frames);
    memcpy(stream, sndData, frames * sizeof(Sound::Frame));
    if (frames * (int)sizeof(Sound::Frame) < len)
        memset(stream + frames * sizeof(Sound::Frame), 0,
               len - frames * sizeof(Sound::Frame));

    Uint32 elapsed = SDL_GetTicks() - start;
    sndMaxMixMS = max(sndMaxMixMS, elapsed);
    if (elapsed >= expected)
        sndLateMixCount++;
}

static bool sndInit() {
    SDL_AudioSpec desired;
    SDL_AudioSpec obtained;
    memset(&desired, 0, sizeof(desired));
    memset(&obtained, 0, sizeof(obtained));

    desired.freq     = SND_FREQ;
    desired.format   = AUDIO_S16SYS;
    desired.channels = 2;
    desired.samples  = SND_FRAMES;
    desired.callback = sndFill;

    if (SDL_OpenAudio(&desired, &obtained) < 0) {
        LOG("SDL_OpenAudio failed: %s\n", SDL_GetError());
        return false;
    }

    if (obtained.freq != desired.freq || obtained.format != desired.format ||
        obtained.channels != desired.channels) {
        puts("OpenLara: unsupported SDL audio format, continuing without sound");
        SDL_CloseAudio();
        return false;
    }

    sndBufferFrames = obtained.samples ? obtained.samples : SND_FRAMES;
    sndData = new Sound::Frame[sndBufferFrames];
    memset(sndData, 0, sndBufferFrames * sizeof(Sound::Frame));

    char info[128];
    snprintf(info, sizeof(info), "AHI format: %d Hz, %d channels, %d frames",
             obtained.freq, (int)obtained.channels, sndBufferFrames);
    traceStartup(info);

    return true;
}

static void sndFree() {
    if (!audioReady)
        return;
    SDL_PauseAudio(1);
    SDL_CloseAudio();
    char info[160];
    snprintf(info, sizeof(info),
             "audio stats: callbacks %lu, max gap %lu ms (%lu late), max mix %lu ms (%lu late)",
             (unsigned long)sndCallbackCount,
             (unsigned long)sndMaxCallbackGapMS,
             (unsigned long)sndLateGapCount,
             (unsigned long)sndMaxMixMS,
             (unsigned long)sndLateMixCount);
    traceStartup(info);
    delete[] sndData;
    sndData = NULL;
    audioReady = false;
}

static void sndStartAfterFirstFrame() {
    if (disableAudio) {
        traceStartup("14 audio: disabled by -nosound");
        return;
    }

    // The Amiga SDL/AHI backend creates a separate task as soon as audio is
    // opened.  On real PiStorm hardware that task must not run while MiniGL is
    // still creating its renderer and uploading the startup resources (WinUAE
    // happens to tolerate that ordering).  Start AHI only after a complete
    // game frame has proved that GAPI and the PiStorm3D context are ready.
    traceStartup("14 late SDL/AHI init: begin");
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
        traceSDLError("15 late SDL/AHI init FAILED");
        return;
    }
    traceStartup("15 late SDL/AHI init: subsystem OK, opening audio");

    audioReady = sndInit();
    if (!audioReady) {
        traceStartup("16 late SDL/AHI init: audio open FAILED, sound disabled");
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return;
    }

    // This SDL 1.2 Amiga backend hard-codes priority 11 for its task.  That can
    // starve the priority-0 game during setup, while lowering audio itself to
    // 0 causes missed deadlines under continuous MiniGL rendering.  Priority
    // 1 gives the short mixer callback precedence without using SDL's overly
    // aggressive value.  Change it only after SDL_OpenAudio has configured
    // the task and before playback is unpaused.
    struct Task *audioTask = FindTask("SDL subtask");
    if (audioTask) {
        int oldPriority = SetTaskPri(audioTask, SND_TASK_PRIORITY);
        char info[96];
        snprintf(info, sizeof(info),
                 "16 SDL/AHI task priority: %d -> %d",
                 oldPriority, SND_TASK_PRIORITY);
        traceStartup(info);
    } else {
        traceStartup("16 SDL/AHI task priority: task NOT FOUND");
    }

    traceStartup("17 SDL/AHI playback: unpause");
    SDL_PauseAudio(0);
    traceStartup("18 SDL/AHI playback: running");
}

static InputKey codeToInputKey(SDLKey code) {
    switch (code) {
        case SDLK_LEFT:       return ikLeft;
        case SDLK_RIGHT:      return ikRight;
        case SDLK_UP:         return ikUp;
        case SDLK_DOWN:       return ikDown;
        case SDLK_SPACE:      return ikSpace;
        case SDLK_TAB:        return ikTab;
        case SDLK_RETURN:     return ikEnter;
        case SDLK_ESCAPE:     return ikEscape;
        case SDLK_LSHIFT:
        case SDLK_RSHIFT:     return ikShift;
        case SDLK_LCTRL:
        case SDLK_RCTRL:      return ikCtrl;
        case SDLK_LALT:
        case SDLK_RALT:       return ikAlt;
        case SDLK_0:          return ik0;
        case SDLK_1:          return ik1;
        case SDLK_2:          return ik2;
        case SDLK_3:          return ik3;
        case SDLK_4:          return ik4;
        case SDLK_5:          return ik5;
        case SDLK_6:          return ik6;
        case SDLK_7:          return ik7;
        case SDLK_8:          return ik8;
        case SDLK_9:          return ik9;
        case SDLK_a:          return ikA;
        case SDLK_b:          return ikB;
        case SDLK_c:          return ikC;
        case SDLK_d:          return ikD;
        case SDLK_e:          return ikE;
        case SDLK_f:          return ikF;
        case SDLK_g:          return ikG;
        case SDLK_h:          return ikH;
        case SDLK_i:          return ikI;
        case SDLK_j:          return ikJ;
        case SDLK_k:          return ikK;
        case SDLK_l:          return ikL;
        case SDLK_m:          return ikM;
        case SDLK_n:          return ikN;
        case SDLK_o:          return ikO;
        case SDLK_p:          return ikP;
        case SDLK_q:          return ikQ;
        case SDLK_r:          return ikR;
        case SDLK_s:          return ikS;
        case SDLK_t:          return ikT;
        case SDLK_u:          return ikU;
        case SDLK_v:          return ikV;
        case SDLK_w:          return ikW;
        case SDLK_x:          return ikX;
        case SDLK_y:          return ikY;
        case SDLK_z:          return ikZ;
        case SDLK_F1:         return ikF1;
        case SDLK_F2:         return ikF2;
        case SDLK_F3:         return ikF3;
        case SDLK_F4:         return ikF4;
        case SDLK_F5:         return ikF5;
        case SDLK_F6:         return ikF6;
        case SDLK_F7:         return ikF7;
        case SDLK_F8:         return ikF8;
        case SDLK_F9:         return ikF9;
        case SDLK_F10:        return ikF10;
        case SDLK_F11:        return ikF11;
        case SDLK_F12:        return ikF12;
        case SDLK_BACKSPACE:  return ikBack;
        default:              return ikNone;
    }
}

static JoyKey joyCodeToJoyKey(int button) {
    switch (button) {
        case 0: return jkA;
        case 1: return jkB;
        case 2: return jkX;
        case 3: return jkY;
        case 4: return jkLB;
        case 5: return jkRB;
        case 6: return jkSelect;
        case 7: return jkStart;
        case 8: return jkL;
        case 9: return jkR;
        default: return jkNone;
    }
}

static float joyAxisValue(int value) {
    if (value > -JOY_DEAD_ZONE && value < JOY_DEAD_ZONE)
        return 0.0f;
    return value / 32767.0f;
}

static vec2 joyDirection(const vec2 &value) {
    float distance = min(1.0f, value.length());
    return value.normal() * distance;
}

static bool inputInit() {
    joyL = joyR = vec2(0.0f);
    memset(joysticks, 0, sizeof(joysticks));
    if (!(SDL_WasInit(SDL_INIT_JOYSTICK) & SDL_INIT_JOYSTICK)) {
        joystickCount = 0;
        return true;
    }
    SDL_JoystickEventState(SDL_ENABLE);
    joystickCount = clamp(SDL_NumJoysticks(), 0, MAX_JOYS);
    for (int i = 0; i < joystickCount; i++)
        joysticks[i] = SDL_JoystickOpen(i);
    return true;
}

static void inputFree() {
    for (int i = 0; i < joystickCount; i++) {
        if (joysticks[i])
            SDL_JoystickClose(joysticks[i]);
        joysticks[i] = NULL;
    }
    joystickCount = 0;
}

static void inputUpdate() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                traceStartup("input: SDL_QUIT");
                Core::isQuit = true;
                break;
            case SDL_KEYDOWN: {
                if (event.key.keysym.sym == SDLK_F10) {
                    traceStartup("input: F10 exit");
                    Core::isQuit = true;
                    return;
                }
                InputKey key = codeToInputKey(event.key.keysym.sym);
                if (key != ikNone) {
                    if (key == ikEscape)
                        traceStartup("input: SDL Escape key down");
                    Input::setDown(key, 1);
                }
                break;
            }
            case SDL_KEYUP: {
                InputKey key = codeToInputKey(event.key.keysym.sym);
                if (key != ikNone)
                    Input::setDown(key, 0);
                break;
            }
            case SDL_JOYBUTTONDOWN:
            case SDL_JOYBUTTONUP: {
                int index = event.jbutton.which;
                if (index >= 0 && index < joystickCount) {
                    JoyKey key = joyCodeToJoyKey(event.jbutton.button);
                    if (key != jkNone)
                        Input::setJoyDown(index, key,
                                          event.type == SDL_JOYBUTTONDOWN);
                }
                break;
            }
            case SDL_JOYAXISMOTION: {
                int index = event.jaxis.which;
                if (index < 0 || index >= joystickCount)
                    break;
                switch (event.jaxis.axis) {
                    case 0: joyL.x = joyAxisValue(event.jaxis.value); break;
                    case 1: joyL.y = joyAxisValue(event.jaxis.value); break;
                    case 2: joyR.x = joyAxisValue(event.jaxis.value); break;
                    case 3: joyR.y = joyAxisValue(event.jaxis.value); break;
                    default: break;
                }
                Input::setJoyPos(index, jkL, joyDirection(joyL));
                Input::setJoyPos(index, jkR, joyDirection(joyR));
                break;
            }
            default:
                break;
        }
    }
}

static void printUsage() {
    puts(versionTag);
    puts("OpenLara-MiniGL [-nosound] [-res WIDTHxHEIGHT] [-fullscreen|-windowed] [-depth BITS] [-d DATA_DIRECTORY] [-l LEVEL_FILE]");
    puts("  -d DIR   directory containing original Tomb Raider data");
    puts("  -l FILE  load a specific level file");
    puts("  -res WxH select a resolution (up to 1920x1080):");
    for (unsigned i = 0; i < COUNT(videoModes); ++i)
        printf("    %dx%d\n", videoModes[i].width, videoModes[i].height);
    puts("  -fullscreen open a fullscreen display");
    puts("  -windowed   open a window (overrides saved fullscreen)");
    puts("  F10         exit immediately without opening the menu");
    puts("  -depth N screen colour depth: 16, 24 or 32 (default and recommended: 32)");
    puts("  -nosound disable SDL/AHI audio for diagnostics");
    puts("  -h       show this help");
}

static int findResolution(const char *text) {
    if (!text)
        return -1;
    char name[24];
    for (int i = 0; i < (int)COUNT(videoModes); i++) {
        snprintf(name, sizeof(name), "%dx%d", videoModes[i].width, videoModes[i].height);
        if (!strcmp(text, name))
            return i;
    }
    return -1;
}

static void readSavedVideoMode() {
    FILE *file = fopen("PROGDIR:settings", "rb");
    if (!file)
        return;

    Core::Settings saved;
    size_t bytes = fread(&saved, 1, sizeof(saved), file);
    fclose(file);

    if (bytes != sizeof(saved) || saved.version != SETTINGS_VERSION)
        return;

    if (saved.resolution < Core::Settings::AMIGA_RES_MAX)
        resolutionIndex = saved.resolution;
    fullscreen = saved.detail.displaymode == Core::Settings::DM_FULLSCREEN;
}

static int parseArguments(int argc, char **argv, char *&levelName) {
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-l")) {
            if (i + 1 >= argc) {
                puts("OpenLara: -l needs a file name");
                return 10;
            }
            levelName = argv[++i];
        } else if (!strcmp(argv[i], "-d")) {
            if (i + 1 >= argc) {
                puts("OpenLara: -d needs a directory");
                return 10;
            }
            strncpy(contentDir, argv[++i], 254);
            contentDir[254] = 0;
        } else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            printUsage();
            return 5;
        } else if (!strcmp(argv[i], "-nosound")) {
            disableAudio = true;

        } else if (!strcmp(argv[i], "-fullscreen")) {
            fullscreen = true;
        } else if (!strcmp(argv[i], "-windowed")) {
            fullscreen = false;
        } else if (!strcmp(argv[i], "-depth")) {
            if (i + 1 >= argc) {
                puts("OpenLara: -depth needs 16, 24 or 32");
                return 10;
            }
            char *end = NULL;
            long value = strtol(argv[++i], &end, 10);
            if (!end || *end || (value != 16 && value != 24 && value != 32)) {
                printf("OpenLara: unsupported screen depth: %s\n", argv[i]);
                printUsage();
                return 10;
            }
            displayDepth = (int)value;
        } else if (!strcmp(argv[i], "-res")) {
            if (i + 1 >= argc) {
                puts("OpenLara: -res needs WIDTHxHEIGHT");
                return 10;
            }
            int value = findResolution(argv[++i]);
            if (value < 0) {
                printf("OpenLara: unsupported resolution: %s\n", argv[i]);
                printUsage();
                return 10;
            }
            resolutionIndex = value;
        } else {
            printf("OpenLara: unknown option: %s\n", argv[i]);
            printUsage();
            return 10;
        }
    }

    size_t len = strlen(contentDir);
    if (len && contentDir[len - 1] != '/' && contentDir[len - 1] != ':' &&
        len < 254) {
        contentDir[len] = '/';
        contentDir[len + 1] = 0;
    }

    return 0;
}

int main(int argc, char **argv) {
    startupLog = fopen("PROGDIR:OpenLara.log", "w");
    atexit(closeStartupLog);
    traceStartup("01 main: start");
    traceStartup(versionTag);

    cacheDir[0] = saveDir[0] = contentDir[0] = 0;
    strcpy(cacheDir, "PROGDIR:");
    strcpy(saveDir,  "PROGDIR:");
    strcpy(contentDir, "PROGDIR:");

    readSavedVideoMode();

    char *levelName = NULL;
    int argResult = parseArguments(argc, argv, levelName);
    if (argResult)
        return argResult == 5 ? 0 : argResult;

    traceStartup("02 MiniGLOpen: begin");
    if (!MiniGLOpen()) {
        traceStartup("02 MiniGLOpen: FAILED");
        return 20;
    }
    traceStartup("03 MiniGLOpen: OK");
    {
        char info[256];
        snprintf(info, sizeof(info),
                 "03 MiniGL library %u.%u, dispatch ABI %lu, flags 0x%08lx",
                 (unsigned)MiniGLBase->lib_Version,
                 (unsigned)MiniGLBase->lib_Revision,
                 (unsigned long)MiniGLDispatch->abiVersion,
                 (unsigned long)MiniGLDispatch->backendFlags);
        traceStartup(info);
    }

    // OpenLara submits one glBegin/glEnd batch per material range.  The
    // MiniGL default holds only 256 vertices, so enlarge it before SDL
    // creates the context; the renderer also chunks larger ranges.
    mglChooseVertexBufferSize(4096);

    traceStartup("04 SDL_Init video: begin");
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        traceSDLError("04 SDL_Init video FAILED");
        // libnix fclose() needs dos.library to remain fully usable.  Do not
        // leave the log to its atexit handler after the graphics libraries
        // have already been torn down.
        closeStartupLog();
        MiniGLClose();
        return 20;
    }
    traceStartup("05 SDL_Init video: OK");

    // Joystick is optional: a missing lowlevel installation must not prevent
    // the renderer and keyboard controls from starting.  AHI is deliberately
    // initialized only after the first complete game frame below.
    if (SDL_InitSubSystem(SDL_INIT_JOYSTICK) < 0)
        traceSDLError("Joystick disabled");

    SDL_GL_SetAttribute(SDL_GL_RED_SIZE,   displayDepth == 16 ? 5 : 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, displayDepth == 16 ? 6 : 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE,  displayDepth == 16 ? 5 : 8);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    // PiStorm3D renders into a native RGBA8/BGRA32 bitmap, so 32-bit is the
    // default and tested mode. -depth allows explicit SDL display-depth
    // experiments; SDL_GL_DEPTH_SIZE above independently controls the Z
    // buffer and always remains 16-bit.
    const VideoMode &videoMode = videoModes[resolutionIndex];
    {
        char info[96];
        snprintf(info, sizeof(info), "06 SDL_SetVideoMode %dx%dx%d %s: begin",
                 videoMode.width, videoMode.height, displayDepth, fullscreen ? "fullscreen" : "windowed");
        traceStartup(info);
    }
    screen = SDL_SetVideoMode(videoMode.width, videoMode.height, displayDepth, SDL_OPENGL | (fullscreen ? SDL_FULLSCREEN : 0));
    if (!screen) {
        traceSDLError("06 SDL_SetVideoMode FAILED");
        closeStartupLog();
        SDL_Quit();
        MiniGLClose();
        return 20;
    }
    traceStartup("07 SDL_SetVideoMode: OK");
    if (!fullscreen)
        centerMiniGLWindow();

    // Submit a known frame before any game or audio initialization.  A black
    // window proves that context creation and the PiStorm3D present path work;
    // a still-grey window means SDL_SetVideoMode did not return or MiniGL did
    // not present the framebuffer.
    glViewport(0, 0, videoMode.width, videoMode.height);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    traceStartup("08 initial black frame: swap begin");
    SDL_GL_SwapBuffers();
    traceStartup("09 initial black frame: swap OK");

    SDL_WM_SetCaption(WND_TITLE, NULL);
    SDL_ShowCursor(SDL_DISABLE);
    Core::width  = screen->w;
    Core::height = screen->h;

    inputInit();
    traceStartup("10 Game::init: begin (audio not started)");
    Game::init(levelName);
    // Keep the menu in sync with a command-line override.  Applying this
    // option writes it to settings; MiniGL uses it on the next launch because
    // changing SDL_SetVideoMode in place destroys all GL resources.
    Core::settings.resolution = resolutionIndex;
    Core::settings.detail.displaymode = fullscreen ? Core::Settings::DM_FULLSCREEN : Core::Settings::DM_WINDOWED;
    traceStartup("11 Game::init: OK");

    traceStartup("12 main loop: begin");
    bool firstFrame = true;
    bool firstPostAudioFrame = true;
    while (!Core::isQuit) {
        Uint32 frameStartMS = SDL_GetTicks();
        bool framePresented = false;
        inputUpdate();
        if (Core::isQuit) break;
        if (osLoadTraceFrames) osTraceLoad("post-load frame %d: update begin", 5 - osLoadTraceFrames);
        if (Game::update()) {
            if (osLoadTraceFrames) osTraceLoad("post-load frame %d: update done, render begin", 5 - osLoadTraceFrames);
            if (Game::render()) {
                if (osLoadTraceFrames) osTraceLoad("post-load frame %d: render done, swap begin", 5 - osLoadTraceFrames);
                SDL_GL_SwapBuffers();
                if (osLoadTraceFrames) {
                    osTraceLoad("post-load frame %d: swap done", 5 - osLoadTraceFrames);
                    --osLoadTraceFrames;
                }
                framePresented = true;
                if (firstFrame) {
                    traceStartup("13 first game frame: presented");
                    firstFrame = false;
                    sndStartAfterFirstFrame();
                } else if (audioReady && firstPostAudioFrame) {
                    traceStartup("19 first post-audio game frame: presented");
                    firstPostAudioFrame = false;
                }
            }
        } else {
            SDL_Delay(1);
        }

        // This MiniGL/SDL backend has no working swap interval.  Respect the
        // existing VSync option with a 50 Hz software cap so a static menu
        // does not spin at 100% CPU and starve the priority-1 AHI callback.
        if (framePresented && Core::settings.detail.vsync) {
            Uint32 elapsed = SDL_GetTicks() - frameStartMS;
            if (elapsed < FRAME_INTERVAL_MS)
                SDL_Delay(FRAME_INTERVAL_MS - elapsed);
        }
    }

    traceStartup("20 cleanup: begin");
    sndFree();
    Game::deinit();
    inputFree();
    traceStartup("21 cleanup: closing log before SDL/MiniGL");
    closeStartupLog();
    puts("22 SDL_Quit: begin");
    SDL_Quit();
    puts("23 SDL_Quit: OK");
    puts("24 MiniGLClose: begin");
    MiniGLClose();
    puts("25 MiniGLClose: OK; returning from main");
    fflush(stdout);
    return 0;
}
