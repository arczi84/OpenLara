AMIGA_ROOT ?= /opt/amiga-gcc6-latest
AMIGA_GCC16_ROOT ?= /opt/amiga-gcc16-bernie
PISTORM3D_ROOT ?= ../../../../../Pistorm3D
MGL_SDK ?= $(PISTORM3D_ROOT)/Pistorm3D_v12
SDL_MGL ?= $(PISTORM3D_ROOT)/tests/sdl-stormmesa-window/libSDL-mgl-v12-windowfix.a
SDL_MGL_INC ?= $(PISTORM3D_ROOT)/libSDL12-mgl/include
SDL_COMPAT_INC ?= $(PISTORM3D_ROOT)/libSDL12-1.2.15.1/include
MGL_RUNTIME ?= $(PISTORM3D_ROOT)/minigl.library
AMIGADATE ?= $(shell date +%-d.%-m.%Y)

CXX := $(AMIGA_ROOT)/bin/m68k-amigaos-g++
NM := $(AMIGA_ROOT)/bin/m68k-amigaos-nm
GCC16_CXX := $(AMIGA_GCC16_ROOT)/bin/m68k-amigaos-g++
GCC16_NM := $(AMIGA_GCC16_ROOT)/bin/m68k-amigaos-nm
TARGET := OpenLara-MiniGL
GCC16_SAFE_TARGET := OpenLara-MiniGL-gcc16-safe
REPO_ROOT := ../../..
DIST_DIR := $(REPO_ROOT)/dist/OpenLara-MiniGL

SOURCES := main.cpp \
	../../libs/stb_vorbis/stb_vorbis.c \
	../../libs/minimp3/minimp3.cpp \
	../../libs/tinf/tinflate.c
OBJECTS := build/main.o build/stb_vorbis.o build/minimp3.o build/tinflate.o
DEPS := $(OBJECTS:.o=.d)
GCC16_SAFE_OBJECTS := build-gcc16-safe/main.o \
	build-gcc16-safe/stb_vorbis.o build-gcc16-safe/minimp3.o \
	build-gcc16-safe/tinflate.o
GCC16_SAFE_DEPS := $(GCC16_SAFE_OBJECTS:.o=.d)

CPPFLAGS := -D__AMIGA_MINIGL__ -DGL_GLEXT_LEGACY \
	-D__AMIGADATE__=\"$(AMIGADATE)\" \
	-I../../ -I$(MGL_SDK)/include -I$(SDL_MGL_INC) -I$(SDL_COMPAT_INC)
CXXFLAGS := -std=gnu++11 -O2 -mcpu=68060 -mhard-float -ffast-math -fno-builtin -mcrt=nix20 \
	-fomit-frame-pointer -fno-strict-aliasing -fno-exceptions -fno-rtti \
	-ffunction-sections -fdata-sections -MMD -MP -Wall -Wno-unused-variable
LDFLAGS := -mcrt=nix20 -Wl,--gc-sections
LDLIBS := -Wl,--start-group $(SDL_MGL) $(MGL_SDK)/libminigl_dispatch.a \
	-Wl,--end-group -lm

# Matches the validated Jazz2 GCC 16.2 "safe math" build.  This toolchain's
# -mhard-float spelling selects the incompatible soft-float libm060 multilib;
# -m68881 enables the same hard-float ABI while selecting the correct runtime.
GCC16_SAFE_CXXFLAGS := -std=gnu++11 -O2 -DNDEBUG -mcpu=68060 -m68881 \
	-mcrt=nix20 -fomit-frame-pointer -fno-exceptions -fno-rtti \
	-fno-optimize-sibling-calls -fno-fast-math -fno-strict-aliasing \
	-funsafe-loop-optimizations -ftree-loop-if-convert-stores \
	-ffunction-sections -fdata-sections -MMD -MP -Wall -Wno-unused-variable
GCC16_SAFE_LDFLAGS := -mcpu=68060 -m68881 -mcrt=nix20 \
	-Wl,--gc-sections

.PHONY: all clean check package gcc16-safe check-gcc16-safe package-gcc16-safe

all: $(GCC16_SAFE_TARGET)

$(TARGET): Makefile $(OBJECTS)
	$(CXX) $(OBJECTS) -o $@ $(LDFLAGS) $(LDLIBS)

build/main.o: main.cpp
	@mkdir -p build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

build/stb_vorbis.o: ../../libs/stb_vorbis/stb_vorbis.c
	@mkdir -p build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

build/minimp3.o: ../../libs/minimp3/minimp3.cpp
	@mkdir -p build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

build/tinflate.o: ../../libs/tinf/tinflate.c
	@mkdir -p build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

gcc16-safe: $(GCC16_SAFE_TARGET)

$(GCC16_SAFE_TARGET): Makefile $(GCC16_SAFE_OBJECTS)
	$(GCC16_CXX) $(GCC16_SAFE_OBJECTS) -o $@ $(GCC16_SAFE_LDFLAGS) $(LDLIBS)

build-gcc16-safe/main.o: main.cpp
	@mkdir -p build-gcc16-safe
	$(GCC16_CXX) $(CPPFLAGS) $(GCC16_SAFE_CXXFLAGS) -c $< -o $@

build-gcc16-safe/stb_vorbis.o: ../../libs/stb_vorbis/stb_vorbis.c
	@mkdir -p build-gcc16-safe
	$(GCC16_CXX) $(CPPFLAGS) $(GCC16_SAFE_CXXFLAGS) -c $< -o $@

build-gcc16-safe/minimp3.o: ../../libs/minimp3/minimp3.cpp
	@mkdir -p build-gcc16-safe
	$(GCC16_CXX) $(CPPFLAGS) $(GCC16_SAFE_CXXFLAGS) -c $< -o $@

build-gcc16-safe/tinflate.o: ../../libs/tinf/tinflate.c
	@mkdir -p build-gcc16-safe
	$(GCC16_CXX) $(CPPFLAGS) $(GCC16_SAFE_CXXFLAGS) -c $< -o $@

check: $(TARGET)
	file $(TARGET)
	@file $(TARGET) | grep -q 'AmigaOS loadseg'
	@undefined="$$($(NM) -u $(TARGET))"; \
		test -z "$$undefined" || { echo "Undefined symbols:"; echo "$$undefined"; exit 1; }
	@strings $(TARGET) | grep -q '\$$VER: OpenLara MiniGL '
	@echo "check: Amiga executable, no unresolved symbols and \$$VER present"

package: check
	@mkdir -p $(DIST_DIR)
	cp $(TARGET) $(DIST_DIR)/
	cp README.md $(DIST_DIR)/README.md
	cp icons/*.info $(DIST_DIR)/
	cp $(MGL_RUNTIME) $(DIST_DIR)/minigl.library
	@echo "package: $(DIST_DIR)"

check-gcc16-safe: $(GCC16_SAFE_TARGET)
	file $(GCC16_SAFE_TARGET)
	@file $(GCC16_SAFE_TARGET) | grep -q 'AmigaOS loadseg'
	@undefined="$$($(GCC16_NM) -u $(GCC16_SAFE_TARGET))"; \
		test -z "$$undefined" || { echo "Undefined symbols:"; echo "$$undefined"; exit 1; }
	@if $(GCC16_NM) $(GCC16_SAFE_TARGET) | grep -q ' ___stkexit$$'; then \
		echo "Unexpected libnix stack-swap teardown in GCC 16.2 binary"; exit 1; \
	fi
	@$(GCC16_NM) $(GCC16_SAFE_TARGET) | grep -q ' ___fpucheck$$'
	@strings $(GCC16_SAFE_TARGET) | grep -q '\$$VER: OpenLara MiniGL '
	@echo "check: GCC 16.2 safe executable, hard-float runtime, no unresolved symbols"

package-gcc16-safe: check-gcc16-safe
	@mkdir -p $(DIST_DIR)
	cp $(GCC16_SAFE_TARGET) $(DIST_DIR)/
	cp README.md $(DIST_DIR)/README.md
	cp icons/*.info $(DIST_DIR)/
	cp $(MGL_RUNTIME) $(DIST_DIR)/minigl.library
	@echo "package: $(DIST_DIR)/$(GCC16_SAFE_TARGET)"

clean:
	$(RM) $(TARGET)
	$(RM) $(GCC16_SAFE_TARGET)
	$(RM) -r build
	$(RM) -r build-gcc16-safe

-include $(DEPS) $(GCC16_SAFE_DEPS)
