#!/bin/bash
# ioq3-on-MiniGLV3D FRESH GCC 6.5.0b (WSL2-native build), MAIN EXECUTABLE
# ONLY -- companion to build_ioq3_gccfresh_wsl_showcase.sh (PERMANENT,
# 2026-08-30, user-directed): skips PHASE 3a/3b/3c (q3_ui, cgame, qagame
# DLLs) and their 3 link steps entirely, since most iteration during
# active engine/renderer work only touches files that get linked into
# the main executable. Still does a FULL fresh recompile of every file
# the main exe needs (see feedback_always_full_rebuild) -- this is NOT
# quick_relink_main.sh's stale-object relink shortcut, which is unsafe
# across struct-layout changes; this script has no such risk, it's just
# narrower in SCOPE (fewer targets), not staler in FRESHNESS.
#
# Runs natively in WSL2 (no Docker) -- invoke via:
#   wsl -e bash /mnt/d/v3d_driver/ioq3-master/build_ioq3_gccfresh_wsl_mainonly.sh
set -e

export PATH="$HOME/amiga-gcc-install/bin:$PATH"
GCC=m68k-amigaos-gcc
AS=m68k-amigaos-as
LIBM020="$HOME/amiga-gcc-install/m68k-amigaos/clib2/lib/libm020"

ROOT=/mnt/d/v3d_driver/ioq3-master
GLINC=/mnt/d/v3d_driver/MiniGLV3D/gl/include
BACKENDINC=/mnt/d/v3d_driver/MiniGLV3D/backend/include
MGLINC="-I$GLINC -I$BACKENDINC"

OUT=/mnt/d/v3d_driver/ioq3-master/ioquake3_objs_68k_gccfresh_showcase
BUILDOUT=/mnt/d/v3d_driver/ioq3-master/build_output
mkdir -p "$OUT" "$BUILDOUT"

CFLAGS="-std=c99 -O2 -finline-functions -mcpu=68020 -m68881 -mcrt=clib2 \
-fno-strict-aliasing -fno-builtin-cos -fno-builtin-sin \
-DAMIGAOS -DNDEBUG -DBOTLIB -DUSE_LOCAL_HEADERS -DDLL -DNO_VM_COMPILED"

cd "$ROOT/code/zlib"
for f in adler32 crc32 inffast inflate inftrees zutil; do
  echo "=== zlib ($f.c) ==="
  $GCC $CFLAGS -c "$f.c" -o "$OUT/$f.o"
done

cd "$ROOT/code/jpeg-8c"
for f in jaricom jcapimin jcapistd jcarith jccoefct jccolor jcdctmgr jchuff \
         jcinit jcmainct jcmarker jcmaster jcomapi jcparam jcprepct jcsample \
         jctrans jdapimin jdapistd jdarith jdatadst jdatasrc jdcoefct jdcolor \
         jddctmgr jdhuff jdinput jdmainct jdmarker jdmaster jdmerge jdpostct \
         jdsample jdtrans jerror jfdctflt jfdctfst jfdctint jidctflt jidctfst \
         jidctint jmemmgr jmemnobs jquant1 jquant2 jutils; do
  echo "=== jpeg-8c ($f.c) ==="
  $GCC $CFLAGS -c "$f.c" -o "$OUT/$f.o"
done

cd "$ROOT/code/qcommon"
for f in cm_load cm_patch cm_polylib cm_test cm_trace cmd common cvar \
         files huffman md4 msg net_chan net_ip q_math \
         q_shared1 q_shared2 q_shared3 q_shared4 q_shared5 q_shared6 \
         q_shared7 q_shared8 md5 puff \
         unzip ioapi vm vm_interpreted; do
  echo "=== qcommon ($f.c) ==="
  $GCC $CFLAGS -c "$f.c" -o "$OUT/$f.o"
done

cd "$ROOT/code/server"
for f in sv_bot sv_ccmds sv_client sv_game sv_init sv_main sv_net_chan \
         sv_snapshot sv_world; do
  echo "=== server ($f.c) ==="
  $GCC $CFLAGS -c "$f.c" -o "$OUT/$f.o"
done

cd "$ROOT/code/botlib"
for f in be_aas_bspq3 be_aas_cluster be_aas_debug be_aas_entity be_aas_file \
         be_aas_main be_aas_move be_aas_optimize be_aas_reach be_aas_route \
         be_aas_routealt be_aas_sample be_ai_chat be_ai_gen \
         be_ai_goal be_ai_move be_ai_weap be_ai_weight be_ea be_interface \
         l_crc l_libvar l_log l_memory l_precomp l_script l_struct; do
  echo "=== botlib ($f.c) [fresh GCC6.5, -O2+inline] ==="
  $GCC $CFLAGS -c "$f.c" -o "$OUT/$f.o"
done

O0FLAGS="-std=c99 -O0 -mcpu=68020 -m68881 -mcrt=clib2 \
-fno-strict-aliasing -fno-builtin-cos -fno-builtin-sin \
-DAMIGAOS -DNDEBUG -DBOTLIB -DUSE_LOCAL_HEADERS -DDLL -DNO_VM_COMPILED"
echo "=== botlib (be_ai_char.c) [PINNED -O0, hardware-confirmed needed] ==="
$GCC $O0FLAGS -c be_ai_char.c -o "$OUT/be_ai_char.o"

cd "$ROOT/code/renderer"
for f in tr_animation tr_backend tr_bsp tr_cmds tr_curve tr_flares tr_font \
         tr_image tr_init tr_light tr_main tr_marks tr_mesh tr_model \
         tr_noise tr_scene tr_shade tr_shade_calc tr_shader tr_shadows \
         tr_sky tr_surface tr_world tr_image_bmp tr_image_tga tr_image_jpg \
         tr_image_png tr_image_pcx; do
  echo "=== renderer ($f.c) ==="
  $GCC $CFLAGS $MGLINC -c "$f.c" -o "$OUT/$f.o"
done

cd "$ROOT/code/client"
for f in cl_cgame cl_cin cl_console cl_input cl_keys cl_main cl_net_chan \
         cl_parse cl_scrn cl_ui cl_avi snd_adpcm snd_dma snd_mem snd_mix \
         snd_wavelet snd_main snd_codec snd_codec_wav; do
  echo "=== client ($f.c) ==="
  $GCC $CFLAGS -c "$f.c" -o "$OUT/$f.o"
done

cd "$ROOT/code/amiga"
for f in amiga_main amiga_input amiga_mhandler amiga_snd amiga_snd_ahi amiga_glimp amiga_shared; do
  echo "=== amiga ($f.c) ==="
  $GCC $CFLAGS $MGLINC -c "$f.c" -o "$OUT/$f.o"
done
echo "=== amiga asm (GAS): snd_int68k_gas.s ==="
$AS -m68020 -m68881 "$ROOT/code/amiga/snd_int68k_gas.s" -o "$OUT/snd_int68k.o"
echo "=== amiga/dll (dll.c only -- loader side) ==="
$GCC $CFLAGS $MGLINC -c "$ROOT/code/amiga/dll/dll.c" -o "$OUT/dll.o"

MGLOPT="-fno-strict-aliasing -fno-builtin-cos -fno-builtin-sin -finline-functions"
MGLBASE="-std=c99 -O2 -mcpu=68020 -m68881 -mcrt=clib2 $MGLOPT $MGLINC"

cd /mnt/d/v3d_driver/MiniGLV3D/gl/src
for f in aclip context dlist draw fog glu hclip init light matrix others texture \
         vertexarray vertexbuffer_min vertexelements viewport; do
  echo "=== MiniGLV3D gl/src ($f.c) ==="
  $GCC $MGLBASE -c "$f.c" -o "$OUT/$f.o"
done
cd /mnt/d/v3d_driver/MiniGLV3D/backend/hw
for f in v3d_assembler v3d_clbuf v3d_commands v3d_context v3d_device v3d_frame v3d_hw \
         v3d_mem_allocvec v3d_submit_timeout v3d_texture; do
  echo "=== MiniGLV3D backend/hw ($f.c) ==="
  $GCC $MGLBASE -c "$f.c" -o "$OUT/$f.o"
done
echo "=== MiniGLV3D backend/hw (v3d_debug.c, -DDEBUG) ==="
$GCC $MGLBASE -DDEBUG -c v3d_debug.c -o "$OUT/v3d_debug.o"
cd /mnt/d/v3d_driver/MiniGLV3D/demos
# mglv3d_log() CONSOLIDATED (2026-09-02): see build_ioq3_gccfresh_wsl_showcase.sh's
# own comment for the full reasoning. mglv3d_log_noop.c by default; swap
# to mglv3d_log_progdir.c for a debug build.
echo "=== MiniGLV3D demos (mglv3d_log_noop.c) ==="
$GCC $MGLBASE -c mglv3d_log_noop.c -o "$OUT/mglv3d_log_noop.o"
echo "=== MiniGLV3D demos (mglv3d_build_version_noop.c) ==="
$GCC $MGLBASE -c mglv3d_build_version_noop.c -o "$OUT/mglv3d_build_version_noop.o"

BOTLIB="be_aas_bspq3 be_aas_cluster be_aas_debug be_aas_entity be_aas_file be_aas_main \
be_aas_move be_aas_optimize be_aas_reach be_aas_route be_aas_routealt be_aas_sample \
be_ai_char be_ai_chat be_ai_gen be_ai_goal be_ai_move be_ai_weap be_ai_weight be_ea \
be_interface l_crc l_libvar l_log l_memory l_precomp l_script l_struct"
CLIENT="cl_avi cl_cgame cl_cin cl_console cl_input cl_keys cl_main cl_net_chan cl_parse \
cl_scrn cl_ui snd_adpcm snd_codec snd_codec_wav snd_dma snd_main snd_mem snd_mix snd_wavelet"
JPEG="jaricom jcapimin jcapistd jcarith jccoefct jccolor jcdctmgr jchuff jcinit jcmainct \
jcmarker jcmaster jcomapi jcparam jcprepct jcsample jctrans jdapimin jdapistd jdarith \
jdatadst jdatasrc jdcoefct jdcolor jddctmgr jdhuff jdinput jdmainct jdmarker jdmaster \
jdmerge jdpostct jdsample jdtrans jerror jfdctflt jfdctfst jfdctint jidctflt jidctfst \
jidctint jmemmgr jmemnobs jquant1 jquant2 jutils"
ZLIB="adler32 crc32 huffman inffast inflate inftrees ioapi puff unzip zutil"
QCOMMON="cm_load cm_patch cm_polylib cm_test cm_trace cmd common cvar files md4 md5 msg \
net_chan net_ip vm vm_interpreted"
RENDERER="tr_animation tr_backend tr_bsp tr_cmds tr_curve tr_flares tr_font tr_image \
tr_image_bmp tr_image_jpg tr_image_pcx tr_image_png tr_image_tga tr_init tr_light tr_main \
tr_marks tr_mesh tr_model tr_noise tr_scene tr_shade tr_shade_calc tr_shader tr_shadows \
tr_sky tr_surface tr_world"
SERVER="sv_bot sv_ccmds sv_client sv_game sv_init sv_main sv_net_chan sv_snapshot sv_world"
AMIGA="amiga_glimp amiga_input amiga_main amiga_mhandler amiga_shared amiga_snd amiga_snd_ahi snd_int68k dll"
SHARED="q_math q_shared1 q_shared2 q_shared3 q_shared4 q_shared5 q_shared6 q_shared7 q_shared8"
MINIGL_SRC="aclip context dlist draw fog glu hclip init light matrix others texture vertexarray \
vertexbuffer_min vertexelements viewport"
MINIGL_HW="v3d_assembler v3d_clbuf v3d_commands v3d_context v3d_debug v3d_device v3d_frame \
v3d_hw v3d_mem_allocvec v3d_submit_timeout v3d_texture"
MINIGL_DEMOS="mglv3d_build_version_noop mglv3d_log_noop"

MAINFILES="$BOTLIB $CLIENT $JPEG $ZLIB $QCOMMON $RENDERER $SERVER $AMIGA $SHARED $MINIGL_SRC $MINIGL_HW $MINIGL_DEMOS"
MAINOBJS=""; for f in $MAINFILES; do MAINOBJS="$MAINOBJS $OUT/$f.o"; done

# FIXED 2026-09-05: this was hardcoded to 228 while the real marker in
# amiga_glimp.c had moved on to 235, so every main-only build emitted a
# binary labelled v228 no matter what it actually contained -- silently
# overwriting the real v228 and mislabelling itself. That is exactly the
# lost-a-test-binary failure the unique-build-name convention exists to
# prevent, and it is the second time version drift from a hardcoded copy has
# bitten this project. Derived from source now, same as the showcase script
# and glquake's, so it cannot drift again.
QUAKE3_BUILD_VERSION=$(grep -oP '(?<=#define QUAKE3_BUILD_VERSION )\d+' /mnt/d/v3d_driver/ioq3-master/code/amiga/amiga_glimp.c)
if [ -z "$QUAKE3_BUILD_VERSION" ]; then
    echo "ERROR: could not read QUAKE3_BUILD_VERSION from code/amiga/amiga_glimp.c" >&2
    exit 1
fi
echo "=== LINK: quake3-68k-v${QUAKE3_BUILD_VERSION} (main only) ==="
$GCC -mcpu=68020 -m68881 -mcrt=clib2 -o "$BUILDOUT/quake3-68k-v${QUAKE3_BUILD_VERSION}" \
  $MAINOBJS -L"$LIBM020" -lm881 -lm -lc -lamiga
echo "LINK exit=$? -- output: $BUILDOUT/quake3-68k-v${QUAKE3_BUILD_VERSION}"
