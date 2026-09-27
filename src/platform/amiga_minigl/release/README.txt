OpenLara MiniGL 1.7 for AmigaOS 3

INSTALLATION
Extract this archive to your games drawer. Put your original Tomb Raider
DATA directory inside OpenLara-MiniGL, beside the executable.
Game data is not included.

Music is distributed separately. Choose ONE archive:
  OpenLara-Music-ADP4.zip - IMA ADPCM music in WAV containers
  OpenLara-Music-WAV.zip  - uncompressed PCM WAV music
Extract your chosen archive to the SAME parent drawer as the port.
Both alternatives install to OpenLara-MiniGL/audio-adp4, the directory
searched by this build. Installing the other alternative replaces the music.
No OGG files are included.

START
Double-click OpenLara. Its project icon uses C:IconX; the script sets a
1 MiB stack and starts OpenLara-MiniGL. From Shell, enter this directory:
  Execute OpenLara

REQUIREMENTS
AmigaOS 3, 68060/FPU or compatible Emu68, RTG, AHI, and a compatible
MiniGL library with dispatch ABI 3 and SDL FromWindow support (v27.2+).
Use the MiniGL backend matching your hardware in LIBS:minigl.library.
No MiniGL runtime is bundled.
The PNG Workbench icon needs PNG icon support in icon.library.

DISPLAY AND CONTROLS
The Details menu offers resolutions up to 1920x1080 and Windowed/Fullscreen.
Apply, then restart the game. Fullscreen requires an available RTG mode.
F10 exits the game.
Command-line options:
  -res WIDTHxHEIGHT  Set resolution, e.g. -res 1920x1080
  -fullscreen        Request fullscreen
  -windowed          Request a window
  -depth 16|24|32    Screen colour depth (default: 32)
  -nosound           Disable audio
  -d DIRECTORY       Use game data from another directory
  -l FILE            Start a specific level
  -h                 Help
If necessary, start with OpenLara-MiniGL -windowed -res 640x480.

Settings, saves and OpenLara.log are written beside the executable.
Logging shows basic version, display and audio information plus errors.
Per-frame, save-loading, input and startup/shutdown traces are removed.

CREDITS
OpenLara: Timur "XProger" Gagiev and contributors.
MorphOS/FFP base: BeWorld2018/OpenLara. Amiga MiniGL port: arczi84.
OpenLara licence: LICENSE.txt. SDL licence: SDL-LICENSE.txt.
Icon attribution is preserved in OpenLara.info.
