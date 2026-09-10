#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>
#include <proto/minigl.h>
#include <clib/minigl_open_protos.h>
#include <mgl/gl.h>
#include <libraries/minigl_dispatch.h>

#define N 256
static unsigned char rgb[N*N*3], rgba[N*N*4];
static FILE *logfile;
static GLuint texture;
static int width = 800, height = 600;
static const char *names[] = {"before-switch", "after-switch", "offset-after-switch", "finish-before-read"};

static void rect(float x0, float y0, float x1, float y1, float r, float g, float b) {
    glColor3f(r,g,b);
    glBegin(GL_QUADS);
    glVertex2f(x0,y0); glVertex2f(x1,y0); glVertex2f(x1,y1); glVertex2f(x0,y1);
    glEnd();
}
static void setup(void) {
    glDisable(GL_DEPTH_TEST); glDisable(GL_BLEND); glDisable(GL_CULL_FACE);
    glDisable(GL_TEXTURE_2D);
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
}
/* Bottom: blue/yellow. Top: red/green. White border, black centre. */
static void pattern(int x, int y, int w, int h) {
    glViewport(x,y,w,h);
    rect(-1,-1,1,1,1,1,1);
    rect(-.9375f,0,0,.9375f,1,0,0);
    rect(0,0,.9375f,.9375f,0,1,0);
    rect(-.9375f,-.9375f,0,0,0,0,1);
    rect(0,-.9375f,.9375f,0,1,1,0);
    rect(-.0625f,-.0625f,.0625f,.0625f,0,0,0);
}
static void capture(int mode) {
    int i, x = mode == 2 ? 96 : 0, y = mode == 2 ? 80 : 0;
    int coords[4][2] = {{64,64},{192,64},{64,192},{192,192}};
    int expected[4][3] = {{0,0,255},{255,255,0},{255,0,0},{0,255,0}};
    char path[120]; FILE *f; GLenum err;
    /* Seed both displayed buffers with a recognisable old frame. */
    setup(); glViewport(0,0,width,height);
    for (i=0;i<2;i++) {
        glClearColor(1,0,1,1); glClear(GL_COLOR_BUFFER_BIT); mglSwitchDisplay();
    }
    glClearColor(.15f,.15f,.15f,1); glClear(GL_COLOR_BUFFER_BIT);
    pattern(x,y,N,N);
    if (mode == 1 || mode == 2) mglSwitchDisplay();
    if (mode == 3) glFinish();
    memset(rgb,0xA5,sizeof(rgb));
    glReadPixels(x,y,N,N,GL_RGB,GL_UNSIGNED_BYTE,rgb);
    err = glGetError();
    fprintf(logfile,"\n%s: rect=%d,%d,%d,%d GL error=0x%lx\n",names[mode],x,y,N,N,(unsigned long)err);
    for (i=0;i<4;i++) {
        unsigned char *p = rgb + (coords[i][1]*N+coords[i][0])*3;
        fprintf(logfile,"sample (%d,%d): %u %u %u; expected %d %d %d\n",
            coords[i][0],coords[i][1],p[0],p[1],p[2],expected[i][0],expected[i][1],expected[i][2]);
    }
    snprintf(path,sizeof(path),"RAM:readpixels-%s.ppm",names[mode]);
    f=fopen(path,"wb");
    if(f) {
        int ok = fprintf(f,"P6\n256 256\n255\n") > 0;
        /* GL rows start at bottom; PPM rows start at top. */
        for(i=N-1;i>=0;i--) if(fwrite(rgb+i*N*3,1,N*3,f)!=N*3) ok=0;
        if(fclose(f)!=0) ok=0;
        fprintf(logfile,"PPM %s: %s\n",path,ok?"OK":"WRITE FAILED");
    } else fprintf(logfile,"Cannot create %s\n",path);
    for(i=0;i<N*N;i++) {
        rgba[i*4]=rgb[i*3]; rgba[i*4+1]=rgb[i*3+1]; rgba[i*4+2]=rgb[i*3+2]; rgba[i*4+3]=255;
    }
    glBindTexture(GL_TEXTURE_2D,texture);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,N,N,0,GL_RGBA,GL_UNSIGNED_BYTE,rgba);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    fprintf(logfile,"RGBA upload GL error=0x%lx\n",(unsigned long)glGetError()); fflush(logfile);
    snprintf(path,sizeof(path),"ReadPixels %d: %s | LEFT reference RIGHT capture",mode+1,names[mode]);
    SDL_WM_SetCaption(path,NULL);
}
static void display(void) {
    setup(); glViewport(0,0,width,height);
    glClearColor(.15f,.15f,.15f,1); glClear(GL_COLOR_BUFFER_BIT);
    pattern(16,32,(width-48)/2,height-64);
    glViewport(width/2+8,32,(width-48)/2,height-64);
    glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D,texture);
    glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);
    glColor4f(1,1,1,1);
    glBegin(GL_QUADS);
    glTexCoord2f(0,0); glVertex2f(-1,-1);
    glTexCoord2f(1,0); glVertex2f(1,-1);
    glTexCoord2f(1,1); glVertex2f(1,1);
    glTexCoord2f(0,1); glVertex2f(-1,1);
    glEnd(); mglSwitchDisplay();
}
int main(int argc,char **argv) {
    SDL_Surface *screen; SDL_Event event; int running=1,mode=1,i,fullscreen=0,depth=16;
    for(i=1;i<argc;i++) {
        if(!strcmp(argv[i],"-fullscreen")) fullscreen=1;
        else if(!strcmp(argv[i],"-32")) depth=32;
        else { puts("Usage: ReadPixelsDemo [-fullscreen] [-32]"); return 20; }
    }
    logfile=fopen("RAM:readpixels-demo.log","w");
    if(!logfile) { puts("Cannot create RAM:readpixels-demo.log"); return 20; }
    if(!MiniGLOpen()) { fprintf(logfile,"MiniGLOpen failed\n"); fclose(logfile); return 20; }
    fprintf(logfile,"MiniGL %u.%u ABI %lu flags 0x%lx\n",(unsigned)MiniGLBase->lib_Version,
        (unsigned)MiniGLBase->lib_Revision,(unsigned long)MiniGLDispatch->abiVersion,(unsigned long)MiniGLDispatch->backendFlags);
    if(SDL_Init(SDL_INIT_VIDEO)<0) goto failed;
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE,depth==16?5:8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE,depth==16?6:8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE,depth==16?5:8);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,16);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);
    screen=SDL_SetVideoMode(width,height,depth,SDL_OPENGL|(fullscreen?SDL_FULLSCREEN:0));
    if(!screen) goto failed;
    width=screen->w; height=screen->h;
    fprintf(logfile,"Surface %dx%d depth %d fullscreen %d\n",width,height,screen->format->BitsPerPixel,fullscreen);
    puts("1: before switch; 2: after switch; 3: offset after switch; 4: glFinish; R: repeat; Esc: exit");
    glGenTextures(1,&texture); capture(mode);
    while(running) {
        while(SDL_PollEvent(&event)) {
            if(event.type==SDL_QUIT) running=0;
            if(event.type==SDL_KEYDOWN) {
                int key=event.key.keysym.sym;
                if(key==SDLK_ESCAPE) running=0;
                if(key>=SDLK_1 && key<=SDLK_4) { mode=key-SDLK_1; capture(mode); }
                if(key==SDLK_r) capture(mode);
            }
        }
        if(running) { display(); SDL_Delay(20); }
    }
    glDeleteTextures(1,&texture); SDL_Quit(); MiniGLClose(); fclose(logfile); return 0;
failed:
    fprintf(logfile,"SDL error: %s\n",SDL_GetError());
    SDL_Quit(); MiniGLClose(); fclose(logfile); return 20;
}
