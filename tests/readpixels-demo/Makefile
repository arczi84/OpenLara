TOOLCHAIN ?= /opt/amiga-gcc16-bernie
PISTORM3D_ROOT ?= ../../../../Pistorm3D
MGL_SDK ?= $(PISTORM3D_ROOT)/Pistorm3D_v12
SDL_MGL ?= $(PISTORM3D_ROOT)/tests/sdl-stormmesa-window/libSDL-mgl-v12-windowfix.a
CC := $(TOOLCHAIN)/bin/m68k-amigaos-gcc
CFLAGS := -std=gnu99 -O2 -mcpu=68060 -m68881 -mcrt=nix20 -fno-strict-aliasing -Wall -Wextra -DGL_GLEXT_LEGACY -I$(MGL_SDK)/include -I$(PISTORM3D_ROOT)/libSDL12-mgl/include
LDLIBS := -Wl,--start-group $(SDL_MGL) $(MGL_SDK)/libminigl_dispatch.a -Wl,--end-group -lm
ReadPixelsDemo: main.c Makefile
	$(CC) $(CFLAGS) main.c -o $@ $(LDLIBS)
.PHONY: clean
clean:
	rm -f ReadPixelsDemo
