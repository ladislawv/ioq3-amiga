Quake III Arena with ioquake3's OpenGL ES 2.0 renderer, for AmigaOS 3
======================================================================

Quake III Arena for 68k AmigaOS 3 on a PiStorm, drawn by the Raspberry Pi
4's GPU through ogles2.library: ioquake3's modern renderer (renderergl2)
with per-pixel shaders, in place of MiniGL. About 30% faster than the
MiniGL build (timedemo "four" at 640 x 480: 80.4 against 61.3 fps on an
A1200 with a Pi 4).

The engine is Cowcat's AmigaOS 3 port of ioquake3 with the PiStorm changes;
see github.com/ladislawv/ioq3-amiga.

Needs
-----
- An Amiga with a PiStorm and a Raspberry Pi 4B or CM4, Emu68
- AmigaOS 3.x with Picasso96 or CyberGraphX, a 32-bit RTG screen mode
- Quake III Arena's data from a retail copy (baseq3/pak0.pk3 to pak8.pk3,
  with the point release). None is included.

Installing
----------
Copy everything in this drawer into your Quake III Arena directory (the one
that holds baseq3/), so that quake3-gl2, libs/ and shaders/ are beside
baseq3/ and the three .dll files land in baseq3/.

Nothing else needs installing: ogles2.library and the shader compiler are
in libs/ here, and the renderer's shaders come compiled in shaders/ for
640 x 480 and 1920 x 1080. If ogles2.library 1.3 or newer is installed in
LIBS:, that one is used.

Playing
-------
From a Shell, in the Quake III Arena directory:

  quake3-gl2 +set r_fullscreen 1 +set r_mode 3

or start the Quake3-GLES2 script there (Execute Quake3-GLES2). The video
menu offers every 32-bit RTG mode of 640 x 480 or more; 1920 x 1080 works.

Other screen modes: the renderer's shaders depend on the screen size, so a
mode other than the two shipped ones compiles its shaders once at its first
start (about 90 to 110 s, with a progress bar), using the compiler in libs/.
They are kept in OGLES2Cache: when that assign exists (ogles2's installer
makes it), otherwise in T: until the next reboot.

Normal and specular mapping are off by default (Quake III's data has no
such maps, so they only cost); the menu can turn them on.

Licences
--------
Quake III Arena's engine and this renderer: GPL v2 or later (COPYING.txt);
the complete source is at github.com/ladislawv/ioq3-amiga.
ogles2.library: Hyperion MiniGL Open Source License; the shader compiler
(libs/v3dglslc) is Mesa, MIT; see licences/ and github.com/ladislawv/ogles2.
Quake III Arena is a trademark of id Software; this project is not
affiliated with or endorsed by id Software or ZeniMax.
