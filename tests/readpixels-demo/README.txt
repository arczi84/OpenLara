MiniGL glReadPixels diagnostic - AmigaOS 3 / 68060 FPU

Run from Shell:
  ReadPixelsDemo
Optional:
  ReadPixelsDemo -fullscreen
  ReadPixelsDemo -32

Uses installed minigl.library through the same V12 dispatch SDK and
SDL windowfix library as the OpenLara MiniGL port. No game data needed.
Window: 800x600, default depth 16 bits.

LEFT: directly drawn reference. RIGHT: RGB readback uploaded as RGBA8.
Expected: white border; red/green on top; blue/yellow below; black centre.
The two panels should match when readback captures the new pattern.
The capture itself is 256x256 and intentionally enlarged for inspection.

Keys:
  1 - draw pattern, read before display switch
  2 - draw pattern, switch display, then read (default; OpenLara sequence)
  3 - same as 2, but viewport AND read rectangle start at (96,80)
  4 - draw pattern, glFinish, read before display switch
  R - repeat current test
  Esc - exit

Before each test both displayed buffers are cleared to magenta.
A magenta capture indicates an old frame; which pre/post-switch tests
work depends on the backend's read buffer behavior. No glReadBuffer
selection is imposed: this deliberately reproduces the port's default.
Grey around a clipped pattern suggests coordinate/viewport disagreement.
0xA5 (165,165,165) samples can indicate an unwritten readback buffer.
Mode 3 must preserve the same pattern as mode 2 if coordinates work.

Files (overwritten on repeat):
  PROGDIR:readpixels-demo.log
  PROGDIR:readpixels-before-switch.ppm
  PROGDIR:readpixels-after-switch.ppm
  PROGDIR:readpixels-offset-after-switch.ppm
  PROGDIR:readpixels-finish-before-read.ppm

PPM contains raw RGB readback before texture upload, with row order
converted from GL bottom-first to PPM top-first. Four interior sample
values and their expected RGB values are logged. Small differences can
result from 16-bit framebuffer precision. GL errors are logged per stage.
If PPM is correct but the right panel is wrong, inspect texture upload
or drawing. If PPM is wrong, inspect capture coordinates/buffer/rendering.
The left panel must first be correct for the reference test to be useful.

Run all four modes on Classic and PiStorm3D. Copy the output files to separate
folders before switching libraries or rebooting. Compare windowed and
fullscreen mode if the capture includes window borders or a grey strip.

Build: make
Source: main.c. Toolchain/SDK paths can be overridden in make variables.
Cross-compiled with GCC 16, -mcpu=68060 -m68881.
Hardware result: modes 1 and 4 reproduced the pattern; mode 2 returned
the old magenta frame. Replacing the pre-read display switch in OpenLara
with glFinish fixed the inventory background in the user's hardware test.
