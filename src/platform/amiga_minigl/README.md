# OpenLara for AmigaOS 3 / PiStorm3D MiniGL

This target uses the fixed-function OpenGL renderer, the MiniGL v12 dispatch
SDK and the SDL 1.2 library built for that SDK.  It is intentionally separate
from the MorphOS/SDL2 target.

The SDL display is explicitly requested in 32-bit colour, matching PiStorm3D's
RGBA8/BGRA32 render target. The separate 16-bit setting is only the depth (Z)
buffer.

The default dependency layout expects the `OpenLara` and `Pistorm3D`
repositories to be siblings. Override `PISTORM3D_ROOT`, `MGL_SDK`, `SDL_MGL`,
`SDL_MGL_INC`, `SDL_COMPAT_INC` or `MGL_RUNTIME` for another layout. The SDL
archive must contain the MiniGL window-adoption and close-event fix: SDL must
not add a close gadget by setting `WFLG_CLOSEGADGET` on an already open
Intuition window.

Build with GCC 16.2 and the Dethrace renderfix-hedeon Release flags:

```sh
make -j4 -C src/platform/amiga_minigl check-gcc16-safe
```

The historical output name remains `OpenLara-MiniGL-gcc16-safe`.
Version 1.6 uses `-O3 -m68060 -mhard-float -fbbb=- -fno-strict-aliasing
-fno-unroll-loops -fomit-frame-pointer -noixemul`, matching Dethrace's
Release code-generation profile. This replaces the earlier Jazz2-derived
profile; hardware validation of 1.5 does not cover these changed flags.
The SDL `kprintf` alias requires `-Wl,-u,_KPrintF` with section garbage
collection; keep this linker fix when changing optimisation flags.

A legacy GCC 6.5 comparison executable can be built separately:

```sh
make -j16 -C src/platform/amiga_minigl package
```

The legacy target retains its separate compiler flags for comparison.

The compiler roots default to `/opt/amiga-gcc6-latest` and
`/opt/amiga16-copy`; override `AMIGA_ROOT` or `AMIGA_GCC16_ROOT` when
needed.

`package` creates `dist/OpenLara-MiniGL` with the executable, this document
and the matching runtime. On the Amiga, install `minigl.library` in
`LIBS:minigl.library`. It is not loaded from the program directory. If the
matching PiStorm3D MiniGL is already installed, it does not need replacing.
Six PNG-icon variants in 64x64, 80x80 and 90x90 sizes are included in the
package under the `OpenLara_1_*` and `OpenLara_2_*` names.

Put the original Tomb Raider data beside the executable, preserving its
directory layout, or pass the data directory explicitly:

```text
OpenLara-MiniGL -d Work:Games/TombRaider
```

Use `-l FILE` to start a specific supported level and `-h` for the complete
command-line help.  No original game data is included in the package.

Basic version, display and audio information and errors are printed to the
console and automatically written to
OpenLara.log beside the executable (PROGDIR:), without per-frame or startup/shutdown tracing. SDL/AHI is opened only after the first rendered game frame,
because starting its task during MiniGL initialization can stall real PiStorm
hardware even though that ordering works in WinUAE.  Running with `-nosound`
skips SDL/AHI initialization completely.

The linked Amiga SDL 1.2 backend gives its audio task priority 11.  This port
starts AHI only after the first rendered frame, then reduces that task to
priority 1 before playback.  This keeps the mixer ahead of its deadline
without allowing it to starve the priority-0 game task during startup.
The AHI mix buffer contains 2048 stereo frames (about 46 ms at 44.1 kHz),
which avoids underruns seen with the desktop-oriented 512-frame setting.

The Details menu offers 320x240, 512x384, 640x480, 800x600, 1024x768,
1280x960, 960x540, 1024x576, 1280x720, 1280x800, 1280x1024, 1366x768,
1440x900, 1600x900, 1680x1050 and 1920x1080. Existing saved resolution IDs
are preserved. Select Windowed or Fullscreen with the Mode option.
Choose Apply, then restart the game for either display setting to take effect.
The selected fullscreen mode must be available in the RTG driver.

Command-line overrides: `-res WIDTHxHEIGHT`, `-fullscreen`, `-windowed`.
If a saved fullscreen mode is unavailable, use `-windowed -res 640x480`.
Press F10 to exit immediately without going through the menu; normal cleanup
still closes audio and the graphics context.

The screen colour depth can be selected with `-depth 16`, `-depth 24` or
`-depth 32`. The default and PiStorm3D-tested value is 32. This setting is
independent from the 16-bit OpenGL Z buffer.

MiniGL's SDL swap does not wait for vertical blank.  With VSync enabled (the
default), the Amiga main loop therefore uses a 50 FPS software cap.  This
prevents menus from spinning at 100% CPU and gives the priority-1 AHI mixer a
regular scheduling window.  Turning VSync off disables the cap.

Use `Stack 1048576` when launching from Shell. The release Workbench icon
requests 1 MiB. This build does not force libnix stack swapping.

The PC/GOG release does not contain the PSX/Saturn area loading screens.  The
upstream `level/1/*.PNG` names are Web-server fallbacks, not missing level
data; this port silently skips them when no native loading screen is present.

PiStorm3D MiniGL has no off-screen framebuffer target.  This port captures a
256x256 inventory background after glFinish, before the display swap and uses a neutral
sphere-map fallback for the few dynamic reflection-bake effects.  This avoids
showing six intermediate reflection views or copying stale frames.
