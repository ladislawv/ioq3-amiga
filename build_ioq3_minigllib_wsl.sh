#!/bin/bash
# ioq3 as a minigl.library CLIENT (2026-10-01). The sibling of
# build_ioq3_gccfresh_wsl_showcase.sh, which compiles the driver INTO the
# executable; this one links the import stub and calls into the resident
# library instead, the way glquakelib, Quake2's ref_gl_shared and glxsglut
# already do. ioq3 was the one port with no library build.
#
# Only the MAIN executable differs. cgame/qagame/ui touch no GL, so this
# script builds and links the main exe alone -- run the showcase script for
# the three DLLs.
#
# Three things a library client needs that the static build does not:
#   -DUSE_MINIGL_LIBRARY   gates the two places amiga_glimp.c reaches into
#                          the driver: the MiniGLOpen() call it now makes at
#                          GLW_LoadOpenGL, and g_mglv3d_last_render_wait_result,
#                          a driver-internal global the library does not export
#   include order          minigl-shared-library/include FIRST, so <mgl/gl.h>
#                          resolves to the client-facing header
#   -lminigl               the import stub, whose GL* entry points forward
#                          through the dispatch table
#
# mglv3d_log_noop.o is still linked: ioq3 calls mglv3d_log() from amiga_glimp.c
# and tr_main.c, and that symbol lives in the driver, not in the library's
# dispatched surface.
set -e
export PATH="$HOME/amiga-gcc-install/bin:$PATH"
GCC=m68k-amigaos-gcc
AS=m68k-amigaos-as
LIBM020="$HOME/amiga-gcc-install/m68k-amigaos/clib2/lib/libm020"

ROOT=/mnt/d/v3d_driver/ioq3-master
GLINC=/mnt/d/v3d_driver/MiniGLV3D/gl/include
BACKENDINC=/mnt/d/v3d_driver/MiniGLV3D/backend/include
SHAREDINC=/mnt/d/v3d_driver/minigl-shared-library/include
SHAREDLIB=/mnt/d/v3d_driver/minigl-shared-library/lib
# client-facing headers FIRST -- they are what a library client compiles against
MGLINC="-I$SHAREDINC -I$GLINC -I$BACKENDINC -I$ROOT/code/amiga"

OUT=/mnt/d/v3d_driver/ioq3-master/ioquake3_objs_68k_minigllib
CGOUT=$OUT/cgame
QAOUT=$OUT/qagame
UIOUT=$OUT/q3_ui
BUILDOUT=/mnt/d/v3d_driver/ioq3-master/build_output_lib
mkdir -p "$OUT" "$CGOUT" "$QAOUT" "$UIOUT" "$BUILDOUT"

CFLAGS="-std=c99 -O2 -finline-functions -mcpu=68020 -m68881 -mcrt=clib2 \
-fno-strict-aliasing -fno-builtin-cos -fno-builtin-sin \
-DAMIGAOS -DNDEBUG -DBOTLIB -DUSE_LOCAL_HEADERS -DDLL -DNO_VM_COMPILED -DUSE_MINIGL_LIBRARY"

# ---------------------------------------------------------------------
# PHASE 1: main executable's own library components (shared $OUT dir)
# ---------------------------------------------------------------------
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

# PINNED (2026-08-29, bots-not-spawning investigation): be_ai_char.c
# hardware-confirmed to need -O0 specifically on this toolchain/source
# state -- real bisection (O0/O2/O3 variants of this one file, rest of
# the build unchanged at -O2+finline) showed bots spawn correctly ONLY
# at -O0. Same real-optimization-bug class this file has hit before
# (see project_minigl_v3d_ioq3_o3_bots_missing memory for the earlier,
# separately-confirmed -O3-specific instance on the OLD native
# toolchain) -- not explained by anything else fixed this session, since
# the corrupted-state_buf bug never showed a grow event for this
# specific symptom's own logs.
O0FLAGS="-std=c99 -O0 -mcpu=68020 -m68881 -mcrt=clib2 \
-fno-strict-aliasing -fno-builtin-cos -fno-builtin-sin \
-DAMIGAOS -DNDEBUG -DBOTLIB -DUSE_LOCAL_HEADERS -DDLL -DNO_VM_COMPILED -DUSE_MINIGL_LIBRARY"
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

# ---------------------------------------------------------------------
# PHASE 2: code/amiga/*.c platform glue (main executable only)
# ---------------------------------------------------------------------
cd "$ROOT/code/amiga"
for f in amiga_main amiga_input amiga_mhandler amiga_snd amiga_snd_ahi amiga_glimp amiga_shared; do
  echo "=== amiga ($f.c) ==="
  $GCC $CFLAGS $MGLINC -c "$f.c" -o "$OUT/$f.o"
done
echo "=== amiga asm (GAS): snd_int68k_gas.s ==="
$AS -m68020 -m68881 "$ROOT/code/amiga/snd_int68k_gas.s" -o "$OUT/snd_int68k.o"
echo "=== amiga/dll (dll.c only -- loader side) ==="
$GCC $CFLAGS $MGLINC -c "$ROOT/code/amiga/dll/dll.c" -o "$OUT/dll.o"

# ---------------------------------------------------------------------
# PHASE 3a: q3_ui module (ui68k.dll) -- own object subdir
# ---------------------------------------------------------------------
UICFLAGS="$CFLAGS"
cd "$ROOT/code/q3_ui"
for f in ui_addbots ui_atoms ui_cdkey ui_cinematics ui_confirm ui_connect \
         ui_controls2 ui_credits ui_demo2 ui_display ui_gameinfo ui_ingame \
         ui_loadconfig ui_main ui_menu ui_mfield ui_mods ui_network \
         ui_options ui_playermodel ui_players ui_playersettings \
         ui_preferences ui_qmenu ui_removebots ui_saveconfig ui_serverinfo \
         ui_servers2 ui_setup ui_sound ui_sparena ui_specifyserver \
         ui_specifyleague ui_splevel ui_sppostgame ui_spskill \
         ui_startserver ui_team ui_teamorders ui_video ui_login \
         ui_rankings ui_spreset ui_rankstatus ui_signup; do
  echo "=== q3_ui ($f.c) ==="
  $GCC $UICFLAGS -c "$f.c" -o "$UIOUT/$f.o"
done
cd "$ROOT/code/ui"
echo "=== ui (ui_syscalls.c) ==="
$GCC $UICFLAGS -c ui_syscalls.c -o "$UIOUT/ui_syscalls.o"
cd "$ROOT/code/qcommon"
for f in q_math q_shared1 q_shared2 q_shared3 q_shared4 q_shared5 q_shared6 \
         q_shared7 q_shared8; do
  echo "=== qcommon ($f.c) [q3_ui-local copy] ==="
  $GCC $UICFLAGS -c "$f.c" -o "$UIOUT/$f.o"
done
cd "$ROOT/code/game"
echo "=== game (bg_misc.c) [q3_ui-local copy] ==="
$GCC $UICFLAGS -c bg_misc.c -o "$UIOUT/bg_misc.o"
cd "$ROOT/code/amiga"
echo "=== amiga (amiga_ui.c) ==="
$GCC $UICFLAGS -c amiga_ui.c -o "$UIOUT/amiga_ui.o"
cd "$ROOT/code/amiga/dll"
echo "=== amiga/dll (dll.c, dllimport.c, dllstartup.c) [q3_ui, loaded module] ==="
for f in dll dllimport dllstartup; do
  $GCC $UICFLAGS -c "$f.c" -o "$UIOUT/$f.o"
done

# ---------------------------------------------------------------------
# PHASE 3b: cgame module (cgame68k.dll) -- own object subdir
# ---------------------------------------------------------------------
cd "$ROOT/code/cgame"
for f in cg_main cg_consolecmds cg_draw cg_drawtools cg_effects cg_ents \
         cg_event cg_info cg_localents cg_marks cg_players cg_playerstate \
         cg_predict cg_scoreboard cg_servercmds cg_snapshot cg_syscalls \
         cg_view cg_weapons cg_particles; do
  echo "=== cgame ($f.c) ==="
  $GCC $CFLAGS -c "$f.c" -o "$CGOUT/$f.o"
done
cd "$ROOT/code/game"
for f in bg_lib bg_misc bg_pmove bg_slidemove; do
  echo "=== game ($f.c) [cgame-local copy] ==="
  $GCC $CFLAGS -c "$f.c" -o "$CGOUT/$f.o"
done
cd "$ROOT/code/qcommon"
for f in q_math q_shared1 q_shared2 q_shared3 q_shared4 q_shared5 q_shared6 \
         q_shared7 q_shared8; do
  echo "=== qcommon ($f.c) [cgame-local copy] ==="
  $GCC $CFLAGS -c "$f.c" -o "$CGOUT/$f.o"
done
cd "$ROOT/code/ui"
echo "=== ui (ui_shared.c) ==="
$GCC $CFLAGS -c ui_shared.c -o "$CGOUT/ui_shared.o"
cd "$ROOT/code/amiga"
echo "=== amiga (amiga_cgame.c) ==="
$GCC $CFLAGS -c amiga_cgame.c -o "$CGOUT/amiga_cgame.o"
cd "$ROOT/code/amiga/dll"
echo "=== amiga/dll (dll.c, dllimport.c, dllstartup.c) [cgame, loaded module] ==="
for f in dll dllimport dllstartup; do
  $GCC $CFLAGS -c "$f.c" -o "$CGOUT/$f.o"
done

# ---------------------------------------------------------------------
# PHASE 3c: qagame module (qagame68k.dll) -- own object subdir
# ---------------------------------------------------------------------
cd "$ROOT/code/amiga"
echo "=== amiga (amiga_game.c) ==="
$GCC $CFLAGS -c amiga_game.c -o "$QAOUT/amiga_game.o"
cd "$ROOT/code/game"
for f in g_main ai_chat ai_cmd ai_dmnet ai_dmq3 ai_main ai_team ai_vcmd \
         bg_misc bg_pmove bg_slidemove g_active g_arenas g_bot g_client \
         g_cmds g_combat g_items g_mem g_misc g_missile g_mover g_session \
         g_spawn g_svcmds g_syscalls g_target g_team g_trigger g_utils \
         g_weapon; do
  echo "=== game ($f.c) ==="
  $GCC $CFLAGS -c "$f.c" -o "$QAOUT/$f.o"
done
cd "$ROOT/code/qcommon"
for f in q_math q_shared1 q_shared2 q_shared3 q_shared4 q_shared5 q_shared6 \
         q_shared7 q_shared8; do
  echo "=== qcommon ($f.c) [qagame-local copy] ==="
  $GCC $CFLAGS -c "$f.c" -o "$QAOUT/$f.o"
done
cd "$ROOT/code/amiga/dll"
echo "=== amiga/dll (dll.c, dllimport.c, dllstartup.c) [qagame, loaded module] ==="
for f in dll dllimport dllstartup; do
  $GCC $CFLAGS -c "$f.c" -o "$QAOUT/$f.o"
done

# ---------------------------------------------------------------------
# PHASE 4: the two driver STUB objects only -- no driver sources.
# mglv3d_log() and the build-version symbol are referenced by ioq3's own
# code but live in the driver, which this build does not link.
# ---------------------------------------------------------------------
MGLOPT="-fno-strict-aliasing -fno-builtin-cos -fno-builtin-sin -finline-functions"
MGLBASE="-std=c99 -O2 -mcpu=68020 -m68881 -mcrt=clib2 $MGLOPT $MGLINC"
cd /mnt/d/v3d_driver/MiniGLV3D/demos
echo "=== MiniGLV3D demos (mglv3d_log_noop.c) ==="
$GCC $MGLBASE -c mglv3d_log_noop.c -o "$OUT/mglv3d_log_noop.o"
echo "=== MiniGLV3D demos (mglv3d_build_version_noop.c) ==="
$GCC $MGLBASE -c mglv3d_build_version_noop.c -o "$OUT/mglv3d_build_version_noop.o"

# ---------------------------------------------------------------------
# LINK all four
# ---------------------------------------------------------------------
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
# No MINIGL_SRC / MINIGL_HW here: the driver is the resident library, not
# objects in this executable. Only the two stubs ioq3 references directly.
MINIGL_DEMOS="mglv3d_build_version_noop mglv3d_log_noop"

MAINFILES="$BOTLIB $CLIENT $JPEG $ZLIB $QCOMMON $RENDERER $SERVER $AMIGA $SHARED $MINIGL_DEMOS"
MAINOBJS=""; for f in $MAINFILES; do MAINOBJS="$MAINOBJS $OUT/$f.o"; done

# Derived from the source marker, NOT hardcoded (2026-09-02). It used to be a
# literal with a "must match amiga_glimp.c" comment, and it silently drifted:
# the source was bumped to 233 while this stayed 232, so the link overwrote the
# existing v232 binary with one that reports 233 at runtime -- exactly the
# lost-a-test-binary gap the unique-build-name convention exists to prevent.
# Same derivation glquake's build script has always used.
QUAKE3_BUILD_VERSION=$(grep -oP '(?<=#define QUAKE3_BUILD_VERSION )\d+' /mnt/d/v3d_driver/ioq3-master/code/amiga/amiga_glimp.c)
if [ -z "$QUAKE3_BUILD_VERSION" ]; then
    echo "ERROR: could not read QUAKE3_BUILD_VERSION from code/amiga/amiga_glimp.c" >&2
    exit 1
fi
echo "=== LINK: quake3-68k-minigllib-v${QUAKE3_BUILD_VERSION} (main, library client) ==="
# FIX (2026-08-27, bsdsocket-not-installed hard-crash investigation): -lnet
# (clib2's separate "libnet" object set) pulled in a startup CONSTRUCTOR
# (socket_init_exit.c) that unconditionally calls OpenLibrary("bsdsocket.
# library",3) before main() ever runs and hard-fails (blocking error,
# refuses to start) if that fails -- completely independent of this game's
# own graceful SocketBase handling (amiga_main.c/net_ip.c), which already
# tolerates a missing bsdsocket.library for local/bot-only play. Verified
# nothing in this codebase actually needs anything from -lnet: net_ip.c's
# own bsdsocket calls dispatch through <proto/bsdsocket.h> using this
# game's own SocketBase, and it already locally reimplements getaddrinfo/
# freeaddrinfo/getnameinfo/gai_strerror -- inet_ntoa was the one remaining
# function actually pulled from -lnet, now also reimplemented locally
# (net_ip.c) since it's pure string formatting with no real socket I/O.
$GCC -mcpu=68020 -m68881 -mcrt=clib2 -o "$BUILDOUT/quake3-68k-minigllib-v${QUAKE3_BUILD_VERSION}" \
  $MAINOBJS -L"$SHAREDLIB" -lminigl -L"$LIBM020" -lm881 -lm -lc -lamiga
echo "LINK exit=$? -- output: $BUILDOUT/quake3-68k-minigllib-v${QUAKE3_BUILD_VERSION}"


echo "=== main executable built as a minigl.library client ==="
