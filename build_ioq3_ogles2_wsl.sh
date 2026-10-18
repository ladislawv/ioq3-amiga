#!/bin/bash
# Builds Quake III Arena for AmigaOS 3 (68k) with ioquake3's OpenGL ES 2.0
# renderer (code/renderergl2) on ogles2.library: build_ogles2/quake3-gl2 and
# the game modules cgame68k.dll, qagame68k.dll, ui68k.dll. In Linux or WSL:
#
#   OGLES2_SDK=/path/to/SDK bash build_ioq3_ogles2_wsl.sh
#
# Needs bebbo's amiga-gcc 6.5 with clib2 ($TC, default ~/amiga-gcc-install),
# python3, and the ogles2 SDK ($OGLES2_SDK: the SDK/ drawer of
# ogles2-1.3-sdk.zip or -sdk.lha from github.com/ladislawv/ogles2/releases,
# with include/ and lib/libogles2.a). Everything else is in this tree:
#   code/              the engine (Cowcat's port with the PiStorm changes)
#   code/renderergl2/, code/renderercommon/
#                      upstream ioquake3's renderer (commit 83a7762) with
#                      two changes for the PiStorm's GPU
#   code/amiga-gl2/    the glimp (amiga_glimp_gl2.c: the ogles2 context, the
#                      screen modes), the adapter between the engine's and
#                      upstream's renderer interfaces (ioq3_gl2_ref.c), a
#                      compatibility header, the GL headers, and the
#                      generator of the GLES2 glue (gen_glue.py)
# Objects go to build_ogles2/obj; nothing in code/ is written.
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
TC=${TC:-$HOME/amiga-gcc-install}
SDK=${OGLES2_SDK:?set OGLES2_SDK to the ogles2 SDK drawer (include/, lib/libogles2.a)}
OUTDIR=${OUTDIR:-$HERE/build_ogles2}
[ -f "$SDK/lib/libogles2.a" ] && [ -f "$SDK/include/libraries/ogles2_dispatch.h" ] ||
    { echo "$SDK is not the ogles2 SDK (no lib/libogles2.a or include/libraries/ogles2_dispatch.h)"; exit 1; }
GCC=$TC/bin/m68k-amigaos-gcc
AS=$TC/bin/m68k-amigaos-as
LIBM020=$TC/m68k-amigaos/clib2/lib/libm020
# The compiler's paths must not contain spaces: work through a link.
ln -sfn "$HERE" "$HOME/.ioq3-amiga-src"
ln -sfn "$SDK" "$HOME/.ioq3-amiga-sdk"
ROOT=$HOME/.ioq3-amiga-src
SDK=$HOME/.ioq3-amiga-sdk
OUT=$OUTDIR/obj
CGOUT=$OUT/cgame
QAOUT=$OUT/qagame
UIOUT=$OUT/q3_ui
ROUT=$OUT/renderergl2
rm -rf "$OUT"
mkdir -p "$OUT" "$CGOUT" "$QAOUT" "$UIOUT" "$ROUT/glsl"

# -fbbb=abcfhilmnprsz0: bebbo's optimiser without pass e, which drops the
# fintrz of float-to-integer conversions (renderergl2 packed colours of 1.0
# as 0).
CFLAGS="-std=c99 -O2 -finline-functions -mcpu=68020 -m68881 -mcrt=clib2 -fbbb=abcfhilmnprsz0 \
-fno-strict-aliasing -fno-builtin-cos -fno-builtin-sin \
-DAMIGAOS -DNDEBUG -DBOTLIB -DUSE_LOCAL_HEADERS -DNO_VM_COMPILED"
EFLAGS="-DDLL"                                       # the engine and the game modules
AINC="-I$ROOT/code/amiga-gl2/include -I$ROOT/code/amiga" # <mgl/gl.h>: the pointer calls only
RFLAGS="-include $ROOT/code/amiga-gl2/include/ioq3_gl2_compat.h -I$ROOT/code/amiga-gl2/include \
-I$ROOT/code/jpeg-8c -DUSE_INTERNAL_JPEG -DGetRefAPI=GL2_GetRefAPI"

cc() { # source object [extra flags...]
    local src=$1 obj=$2
    shift 2
    $GCC $CFLAGS "$@" -c "$src" -o "$obj" || { echo "FAILED: $src"; exit 1; }
}

# ---- the engine (without code/renderer, MiniGL's)
cd "$ROOT/code/zlib"
for f in adler32 crc32 inffast inflate inftrees zutil; do cc $f.c "$OUT/$f.o" $EFLAGS; done
cd "$ROOT/code/jpeg-8c"
for f in jaricom jcapimin jcapistd jcarith jccoefct jccolor jcdctmgr jchuff \
         jcinit jcmainct jcmarker jcmaster jcomapi jcparam jcprepct jcsample \
         jctrans jdapimin jdapistd jdarith jdatadst jdatasrc jdcoefct jdcolor \
         jddctmgr jdhuff jdinput jdmainct jdmarker jdmaster jdmerge jdpostct \
         jdsample jdtrans jerror jfdctflt jfdctfst jfdctint jidctflt jidctfst \
         jidctint jmemmgr jmemnobs jquant1 jquant2 jutils; do cc $f.c "$OUT/$f.o" $EFLAGS; done
cd "$ROOT/code/qcommon"
for f in cm_load cm_patch cm_polylib cm_test cm_trace cmd common cvar \
         files huffman md4 msg net_chan net_ip q_math \
         q_shared1 q_shared2 q_shared3 q_shared4 q_shared5 q_shared6 \
         q_shared7 q_shared8 md5 puff unzip ioapi vm vm_interpreted; do cc $f.c "$OUT/$f.o" $EFLAGS; done
cd "$ROOT/code/server"
for f in sv_bot sv_ccmds sv_client sv_game sv_init sv_main sv_net_chan \
         sv_snapshot sv_world; do cc $f.c "$OUT/$f.o" $EFLAGS; done
cd "$ROOT/code/botlib"
for f in be_aas_bspq3 be_aas_cluster be_aas_debug be_aas_entity be_aas_file \
         be_aas_main be_aas_move be_aas_optimize be_aas_reach be_aas_route \
         be_aas_routealt be_aas_sample be_ai_chat be_ai_gen \
         be_ai_goal be_ai_move be_ai_weap be_ai_weight be_ea be_interface \
         l_crc l_libvar l_log l_memory l_precomp l_script l_struct; do cc $f.c "$OUT/$f.o" $EFLAGS; done
# be_ai_char.c needs -O0 on this toolchain (the bots).
$GCC $(echo "$CFLAGS" | sed 's/-O2 -finline-functions/-O0/') $EFLAGS -c be_ai_char.c -o "$OUT/be_ai_char.o"
cd "$ROOT/code/client"
for f in cl_cgame cl_cin cl_console cl_input cl_keys cl_main cl_net_chan \
         cl_parse cl_scrn cl_ui cl_avi snd_adpcm snd_dma snd_mem snd_mix \
         snd_wavelet snd_main snd_codec snd_codec_wav; do cc $f.c "$OUT/$f.o" $EFLAGS; done
cd "$ROOT/code/amiga"
for f in amiga_main amiga_input amiga_mhandler amiga_shared amiga_snd amiga_snd_ahi mglv3d_log_noop; do
    cc $f.c "$OUT/$f.o" $EFLAGS $AINC
done
$AS -m68020 -m68881 "$ROOT/code/amiga/snd_int68k_gas.s" -o "$OUT/snd_int68k.o"
cc "$ROOT/code/amiga/dll/dll.c" "$OUT/dll.o" $EFLAGS $AINC

# ---- the game modules (no GL)
cd "$ROOT/code/q3_ui"
for f in ui_addbots ui_atoms ui_cdkey ui_cinematics ui_confirm ui_connect \
         ui_controls2 ui_credits ui_demo2 ui_display ui_gameinfo ui_ingame \
         ui_loadconfig ui_main ui_menu ui_mfield ui_mods ui_network \
         ui_options ui_playermodel ui_players ui_playersettings \
         ui_preferences ui_qmenu ui_removebots ui_saveconfig ui_serverinfo \
         ui_servers2 ui_setup ui_sound ui_sparena ui_specifyserver \
         ui_specifyleague ui_splevel ui_sppostgame ui_spskill \
         ui_startserver ui_team ui_teamorders ui_video ui_login \
         ui_rankings ui_spreset ui_rankstatus ui_signup; do cc $f.c "$UIOUT/$f.o" $EFLAGS; done
cc "$ROOT/code/ui/ui_syscalls.c" "$UIOUT/ui_syscalls.o" $EFLAGS
cc "$ROOT/code/game/bg_misc.c" "$UIOUT/bg_misc.o" $EFLAGS
cc "$ROOT/code/amiga/amiga_ui.c" "$UIOUT/amiga_ui.o" $EFLAGS
cd "$ROOT/code/cgame"
for f in cg_main cg_consolecmds cg_draw cg_drawtools cg_effects cg_ents \
         cg_event cg_info cg_localents cg_marks cg_players cg_playerstate \
         cg_predict cg_scoreboard cg_servercmds cg_snapshot cg_syscalls \
         cg_view cg_weapons cg_particles; do cc $f.c "$CGOUT/$f.o" $EFLAGS; done
for f in bg_lib bg_misc bg_pmove bg_slidemove; do cc "$ROOT/code/game/$f.c" "$CGOUT/$f.o" $EFLAGS; done
cc "$ROOT/code/ui/ui_shared.c" "$CGOUT/ui_shared.o" $EFLAGS
cc "$ROOT/code/amiga/amiga_cgame.c" "$CGOUT/amiga_cgame.o" $EFLAGS
cc "$ROOT/code/amiga/amiga_game.c" "$QAOUT/amiga_game.o" $EFLAGS
cd "$ROOT/code/game"
for f in g_main ai_chat ai_cmd ai_dmnet ai_dmq3 ai_main ai_team ai_vcmd \
         bg_misc bg_pmove bg_slidemove g_active g_arenas g_bot g_client \
         g_cmds g_combat g_items g_mem g_misc g_missile g_mover g_session \
         g_spawn g_svcmds g_syscalls g_target g_team g_trigger g_utils \
         g_weapon; do cc $f.c "$QAOUT/$f.o" $EFLAGS; done
for d in "$UIOUT" "$CGOUT" "$QAOUT"; do
    for f in q_math q_shared1 q_shared2 q_shared3 q_shared4 q_shared5 q_shared6 q_shared7 q_shared8; do
        cc "$ROOT/code/qcommon/$f.c" "$d/$f.o" $EFLAGS
    done
    for f in dll dllimport dllstartup; do cc "$ROOT/code/amiga/dll/$f.c" "$d/$f.o" $EFLAGS; done
done

# ---- the renderer: renderergl2 + renderercommon, and the GLSL as C strings
# (upstream's cmake/utils/stringify_shader.cmake)
GLSL=""
for f in "$ROOT"/code/renderergl2/glsl/*.glsl; do
    n=$(basename "$f" .glsl)
    python3 -I - "$f" "$ROUT/glsl/$n.c" "$n" <<'PY'
import sys
src, out, name = sys.argv[1:4]
text = open(src, encoding="latin-1").read().replace("\r", "")
body = text.replace("\\", "\\\\").replace('"', '\\"').replace("\n", '\\n"\n"')
open(out, "w", encoding="latin-1").write('const char *fallbackShader_%s =\n"%s";\n' % (name, body))
PY
    cc "$ROUT/glsl/$n.c" "$ROUT/glsl/$n.o"
    GLSL="$GLSL $ROUT/glsl/$n.o"
done
RENDERER=""
cd "$ROOT/code/renderergl2"
for f in tr_animation tr_backend tr_bsp tr_cmds tr_curve tr_dsa tr_extensions tr_extramath tr_fbo \
         tr_flares tr_glsl tr_image tr_image_dds tr_init tr_light tr_main tr_marks tr_mesh tr_model \
         tr_model_iqm tr_postprocess tr_scene tr_shade tr_shade_calc tr_shader tr_shadows tr_sky \
         tr_surface tr_vbo tr_world; do
    cc $f.c "$ROUT/$f.o" $RFLAGS
    RENDERER="$RENDERER $ROUT/$f.o"
done
# renderercommon without tr_subs.c (a separately loaded renderer's Com_Printf)
# and puff.c (the engine has it).
cd "$ROOT/code/renderercommon"
for f in tr_font tr_image_bmp tr_image_jpg tr_image_pcx tr_image_png tr_image_pvr tr_image_tga tr_noise; do
    cc $f.c "$ROUT/rc_$f.o" $RFLAGS
    RENDERER="$RENDERER $ROUT/rc_$f.o"
done
cd "$ROOT/code/amiga-gl2"
cc amiga_glimp_gl2.c "$ROUT/amiga_glimp_gl2.o" $RFLAGS -I"$SDK/include"
# The GL functions: generated glue over ogles2.library's dispatch table
# (QUAKE3_GL_PROFILE set: every call timed, the split printed at shutdown).
python3 -I "$ROOT/code/amiga-gl2/gen_glue.py" "$SDK/include/libraries/ogles2_dispatch.h" "$ROUT/ogles2_glue.c" \
    QUAKE3_GL_PROFILE > /dev/null
cc "$ROUT/ogles2_glue.c" "$ROUT/ogles2_glue.o" -I"$SDK/include"
# Not -DGetRefAPI here: this file defines the engine's GetRefAPI and calls GL2_GetRefAPI.
cc ioq3_gl2_ref.c "$ROUT/ioq3_gl2_ref.o" $(echo "$RFLAGS" | sed 's/-DGetRefAPI=GL2_GetRefAPI//')

# ---- link
objs() { local dir=$1; shift; for f in "$@"; do echo -n " $dir/$f.o"; done; }
MAIN=$(objs "$OUT" be_aas_bspq3 be_aas_cluster be_aas_debug be_aas_entity be_aas_file be_aas_main \
  be_aas_move be_aas_optimize be_aas_reach be_aas_route be_aas_routealt be_aas_sample \
  be_ai_char be_ai_chat be_ai_gen be_ai_goal be_ai_move be_ai_weap be_ai_weight be_ea \
  be_interface l_crc l_libvar l_log l_memory l_precomp l_script l_struct \
  cl_avi cl_cgame cl_cin cl_console cl_input cl_keys cl_main cl_net_chan cl_parse \
  cl_scrn cl_ui snd_adpcm snd_codec snd_codec_wav snd_dma snd_main snd_mem snd_mix snd_wavelet \
  jaricom jcapimin jcapistd jcarith jccoefct jccolor jcdctmgr jchuff jcinit jcmainct \
  jcmarker jcmaster jcomapi jcparam jcprepct jcsample jctrans jdapimin jdapistd jdarith \
  jdatadst jdatasrc jdcoefct jdcolor jddctmgr jdhuff jdinput jdmainct jdmarker jdmaster \
  jdmerge jdpostct jdsample jdtrans jerror jfdctflt jfdctfst jfdctint jidctflt jidctfst \
  jidctint jmemmgr jmemnobs jquant1 jquant2 jutils \
  adler32 crc32 huffman inffast inflate inftrees ioapi puff unzip zutil \
  cm_load cm_patch cm_polylib cm_test cm_trace cmd common cvar files md4 md5 msg \
  net_chan net_ip vm vm_interpreted \
  sv_bot sv_ccmds sv_client sv_game sv_init sv_main sv_net_chan sv_snapshot sv_world \
  amiga_input amiga_main amiga_mhandler amiga_shared amiga_snd amiga_snd_ahi snd_int68k dll \
  q_math q_shared1 q_shared2 q_shared3 q_shared4 q_shared5 q_shared6 q_shared7 q_shared8 mglv3d_log_noop)
$GCC -mcpu=68020 -m68881 -mcrt=clib2 -o "$OUTDIR/quake3-gl2" $MAIN $RENDERER $GLSL \
    "$ROUT/amiga_glimp_gl2.o" "$ROUT/ioq3_gl2_ref.o" "$ROUT/ogles2_glue.o" \
    -L"$SDK/lib" -logles2 -L"$LIBM020" -lm881 -lm -lc -lamiga 2>&1 | grep -v lang_reloc || true
[ -f "$OUTDIR/quake3-gl2" ] || { echo "link failed"; exit 1; }

CG=$(objs "$CGOUT" cg_main cg_consolecmds cg_draw cg_drawtools cg_effects cg_ents cg_event cg_info \
  cg_localents cg_marks cg_players cg_playerstate cg_predict cg_scoreboard cg_servercmds \
  cg_snapshot cg_syscalls cg_view cg_weapons cg_particles bg_lib bg_misc bg_pmove bg_slidemove \
  q_math q_shared1 q_shared2 q_shared3 q_shared4 q_shared5 q_shared6 q_shared7 q_shared8 \
  ui_shared amiga_cgame dll dllimport dllstartup)
QA=$(objs "$QAOUT" amiga_game g_main ai_chat ai_cmd ai_dmnet ai_dmq3 ai_main ai_team ai_vcmd \
  bg_misc bg_pmove bg_slidemove g_active g_arenas g_bot g_client g_cmds g_combat g_items g_mem \
  g_misc g_missile g_mover g_session g_spawn g_svcmds g_syscalls g_target g_team g_trigger \
  g_utils g_weapon q_math q_shared1 q_shared2 q_shared3 q_shared4 q_shared5 q_shared6 q_shared7 \
  q_shared8 dll dllimport dllstartup)
UI=$(objs "$UIOUT" ui_addbots ui_atoms ui_cdkey ui_cinematics ui_confirm ui_connect ui_controls2 \
  ui_credits ui_demo2 ui_display ui_gameinfo ui_ingame ui_loadconfig ui_main ui_menu ui_mfield \
  ui_mods ui_network ui_options ui_playermodel ui_players ui_playersettings ui_preferences \
  ui_qmenu ui_removebots ui_saveconfig ui_serverinfo ui_servers2 ui_setup ui_sound ui_sparena \
  ui_specifyserver ui_specifyleague ui_splevel ui_sppostgame ui_spskill ui_startserver ui_team \
  ui_teamorders ui_video ui_login ui_rankings ui_spreset ui_rankstatus ui_signup ui_syscalls \
  q_math q_shared1 q_shared2 q_shared3 q_shared4 q_shared5 q_shared6 q_shared7 q_shared8 \
  bg_misc amiga_ui dll dllimport dllstartup)
for m in "cgame68k.dll:$CG" "qagame68k.dll:$QA" "ui68k.dll:$UI"; do
    $GCC -mcpu=68020 -m68881 -mcrt=clib2 -o "$OUTDIR/${m%%:*}" ${m#*:} -L"$LIBM020" -lm881 -lm -lamiga \
        2>&1 | grep -v lang_reloc || true
    [ -f "$OUTDIR/${m%%:*}" ] || { echo "link of ${m%%:*} failed"; exit 1; }
done
ls -l "$OUTDIR"/quake3-gl2 "$OUTDIR"/*.dll
