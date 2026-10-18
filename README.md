# ioq3
ioquake3 for Amiga OS3

A cool attempt to bring Quake3 to the Amiga Classic Platform.

## About this repository

This is **Cowcat's port of ioquake3 to AmigaOS 3 (68k)** (also WarpOS), with
changes for the PiStorm (Raspberry Pi with Emu68) and its V3D GPU. Cowcat's
port is not on GitHub; the code here is his work plus the changes below, and
the credit for the port is his.

Changes in this version (build 296, October 2026):

- **A minigl.library client build** (`build_ioq3_minigllib_wsl.sh`,
  `build_ioq3_minigllib_win.ps1`): the game linked against PiStorm3D's shared
  minigl.library 29.1 (`USE_MINIGL_LIBRARY`, `code/amiga/minigl_library_compat.h`)
  instead of a static MiniGL.
- **Paula sound fixes** (build 294): the mixer wrote stereo interleaved while
  Paula plays the buffer as two halves (wrong pitch, buzz), and buffers were
  cleared to 0x80 on a signed device (thumps) — `client/snd_mix.c`,
  `client/snd_local.h` (`dma.splitchannels`), `client/snd_dma.c`,
  `code/amiga/amiga_snd.c`.
- **An AHI sound backend** with Paula as the automatic fallback (builds
  295-296): `code/amiga/amiga_snd_ahi.c`, the `s_driver` cvar (`ahi`, the
  default, or `paula`), init logging with `s_ahilog 1`.
- Further fixes noted in the sources with their dates (renderer depth state,
  menu surfaces, polygon offset, bots at `-O0` for `be_ai_char.c`).

## The OpenGL ES 2.0 build (ogles2)

Besides the MiniGL build, this tree builds Quake III with **ioquake3's
modern renderer, `renderergl2`**, on [ogles2.library](https://github.com/ladislawv/ogles2)
(OpenGL ES 2.0 on the PiStorm's V3D): per-pixel shaders, and about 30% faster
than MiniGL (timedemo `four`, 640x480, A1200 with a Pi 4: 80.4 against 61.3 fps).

- `code/renderergl2/`, `code/renderercommon/`, `code/qcommon/json.h`: upstream
  ioquake3 at commit `83a7762`, with two changes in `renderergl2`: normal and
  specular mapping off by default (Quake III's data has no such maps), and a
  progress bar while shaders compile on the Amiga.
- `code/amiga-gl2/`: the glimp (`amiga_glimp_gl2.c`: the ogles2 context, the
  32-bit RTG screen modes, the mouse pointer), the adapter between the
  engine's and upstream's renderer interfaces (`ioq3_gl2_ref.c`), a
  compatibility header, the GL headers (types and constants only), the
  generator of the GLES2 glue (`gen_glue.py`), and the package's ReadMe and
  start script.
- `build_ioq3_ogles2_wsl.sh`: builds `build_ogles2/quake3-gl2` and the three
  game modules. Needs bebbo's amiga-gcc 6.5 with clib2 and the ogles2 SDK
  (`OGLES2_SDK`, from ogles2's releases).
- `package_ioq3_ogles2_wsl.sh`: makes the user package `Quake3-GLES2-<build>.lha`
  (the game, `libs/ogles2.library`, the shader compiler `libs/v3dglslc`, the
  renderer's compiled shaders in `shaders/`, ReadMe, licences). With `dist`
  it also copies it into `release/`.

**The built package is in [`release/`](release/)** (`Quake3-GLES2-296.lha`,
with `SHA256SUMS`): the released build, refreshed only when a new build is
released.

The renderer compiles 109 GLSL programs; their compiled form depends on the
screen size. The package ships them for 640x480 and 1920x1080; other modes
compile once at their first start (about 90 to 110 s) with the compiler in
`libs/`.

## Building

The build scripts expect GCC 6.5.0b (bebbo's amiga-gcc) with clib2 and the
PiStorm3D MiniGL SDK; their paths (`D:\v3d_driver\...`, `/mnt/d/...`) are the
original build machine's and need adjusting. Every file that includes
`client/snd_local.h` must be recompiled after a change to `dma_t`.

The game needs the Quake III Arena data (`baseq3/pak*.pk3`) from a retail copy;
none is included.

## Also used by

[ogles2](https://github.com/ladislawv/ogles2) (OpenGL ES 2.0 for AmigaOS 3 on
the PiStorm's V3D) builds this engine with gl4es (`ports/ioq3`) and with
ioquake3's GLES2 renderer (`ports/ioq3-gl2`).

## Licence

GPL v2 or later, as id Software's Quake III Arena source and ioquake3
(`COPYING.txt`). Quake III Arena is a trademark of id Software; this project is
not affiliated with or endorsed by id Software or ZeniMax.
