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

Build the hardware-tested GCC 16.2 safe-math version:

```sh
make -j16 -C src/platform/amiga_minigl package-gcc16-safe
```

It produces `OpenLara-MiniGL-gcc16-safe` using the validated safe-math flags.
The GCC 16.2 toolchain uses `-m68881` deliberately: unlike its
`-mhard-float` alias, it selects the ABI-compatible hard-float runtime.

A legacy GCC 6.5 comparison executable can be built separately:

```sh
make -j16 -C src/platform/amiga_minigl package
```

That compiler currently exhibits gameplay floating-point errors not present
in the GCC 16.2 safe build, so it is retained for comparison rather than
recommended for releases.

The compiler roots default to `/opt/amiga-gcc6-latest` and
`/opt/amiga-gcc16-bernie`; override `AMIGA_ROOT` or `AMIGA_GCC16_ROOT` when
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

For PiStorm3D startup diagnostics, the program writes `OpenLara.log` beside
the executable.  SDL/AHI is opened only after the first rendered game frame,
because starting its task during MiniGL initialization can stall real PiStorm
hardware even though that ordering works in WinUAE.  Running with `-nosound`
skips SDL/AHI initialization completely.

The linked Amiga SDL 1.2 backend gives its audio task priority 11.  This port
starts AHI only after the first rendered frame, then reduces that task to
priority 1 before playback.  This keeps the mixer ahead of its deadline
without allowing it to starve the priority-0 game task during startup.
The AHI mix buffer contains 2048 stereo frames (about 46 ms at 44.1 kHz),
which avoids underruns seen with the desktop-oriented 512-frame setting.

The Details menu offers 320x240, 512x384, 640x480, 800x600, 1024x768 and
1280x960 window sizes.  Select a size, choose Apply and restart OpenLara; an
in-place SDL mode change would destroy the MiniGL context and all GL objects.
The same modes can be selected for one launch with `-res WIDTHxHEIGHT`.

The screen colour depth can be selected with `-depth 16`, `-depth 24` or
`-depth 32`. The default and PiStorm3D-tested value is 32. This setting is
independent from the 16-bit OpenGL Z buffer.

MiniGL's SDL swap does not wait for vertical blank.  With VSync enabled (the
default), the Amiga main loop therefore uses a 50 FPS software cap.  This
prevents menus from spinning at 100% CPU and gives the priority-1 AHI mixer a
regular scheduling window.  Turning VSync off disables the cap.

The executable requests a 1 MiB libnix stack itself, so it can be launched
from Shell or Workbench without a separate `Stack` command.

The PC/GOG release does not contain the PSX/Saturn area loading screens.  The
upstream `level/1/*.PNG` names are Web-server fallbacks, not missing level
data; this port silently skips them when no native loading screen is present.

PiStorm3D MiniGL has no off-screen framebuffer target.  This port captures a
256x256 inventory background from a submitted frame and uses a neutral
sphere-map fallback for the few dynamic reflection-bake effects.  This avoids
showing six intermediate reflection views or copying stale frames.
