#!/bin/bash
# Fast iteration helper for main-exe-only changes (bisection checkpoints,
# engine-side logging, driver changes, etc.) -- recompiles ONLY the given
# source file(s) and relinks ONLY the main executable, reusing every other
# already-built .o in $OUT untouched. Skips cgame68k.dll/qagame68k.dll/
# ui68k.dll entirely: their sources (game/, cgame/, q3_ui/, ui/) have their
# own object dirs and need the full build_ioq3_gccfresh_wsl_showcase.sh.
#
# ALIGNED WITH build_ioq3_gccfresh_wsl_showcase.sh (2026-09-13). Each file is
# compiled with the flags, working directory and source spelling that script
# uses for it, and the link names exactly its main-executable objects:
#   botlib/be_ai_char.c                  -O0 (pinned there: bots only spawn at -O0)
#   renderer/*, amiga/*, amiga/dll/dll.c game flags + MiniGL includes
#   zlib, jpeg-8c, qcommon, server,
#   client, the rest of botlib           game flags
#   amiga/snd_int68k_gas.s               assembled -> snd_int68k.o
#   MiniGLV3D gl/src, backend/hw and
#   demos/mglv3d_*_noop.c                the driver's own flags
#   MiniGLV3D backend/hw/v3d_debug.c     the driver's flags + -DDEBUG
# A file whose object is not in the main link is refused. Before this it
# compiled everything with the game flags and linked two objects the full
# build no longer does: a leftover v3d_submit.o and mglv3d_log_ioq3.o (which
# writes a disk log on every launch) in place of mglv3d_log_noop.o.
#
# Usage: bash quick_relink_main.sh <build_version> <file> [more files...]
#   <file> is relative to code/ (server/sv_init.c), or relative to
#   D:\v3d_driver for the driver (MiniGLV3D/gl/src/draw.c).
# Example: bash quick_relink_main.sh 284 renderer/tr_cmds.c MiniGLV3D/gl/src/draw.c
#
# The binary is named from <build_version>; amiga/amiga_glimp.c's
# QUAKE3_BUILD_VERSION is what the game prints, so pass that file too when the
# number moves.
set -e

export PATH="$HOME/amiga-gcc-install/bin:$PATH"
GCC=m68k-amigaos-gcc
AS=m68k-amigaos-as
LIBM020="$HOME/amiga-gcc-install/m68k-amigaos/clib2/lib/libm020"

V3D=/mnt/d/v3d_driver
ROOT=$V3D/ioq3-master
GLINC=$V3D/MiniGLV3D/gl/include
BACKENDINC=$V3D/MiniGLV3D/backend/include
MGLINC="-I$GLINC -I$BACKENDINC"

OUT=$ROOT/ioquake3_objs_68k_gccfresh_showcase
BUILDOUT=$ROOT/build_output

# The full build's flag sets, verbatim.
CFLAGS="-std=c99 -O2 -finline-functions -mcpu=68020 -m68881 -mcrt=clib2 \
-fno-strict-aliasing -fno-builtin-cos -fno-builtin-sin \
-DAMIGAOS -DNDEBUG -DBOTLIB -DUSE_LOCAL_HEADERS -DDLL -DNO_VM_COMPILED"
O0FLAGS="-std=c99 -O0 -mcpu=68020 -m68881 -mcrt=clib2 \
-fno-strict-aliasing -fno-builtin-cos -fno-builtin-sin \
-DAMIGAOS -DNDEBUG -DBOTLIB -DUSE_LOCAL_HEADERS -DDLL -DNO_VM_COMPILED"
MGLOPT="-fno-strict-aliasing -fno-builtin-cos -fno-builtin-sin -finline-functions"
MGLBASE="-std=c99 -O2 -mcpu=68020 -m68881 -mcrt=clib2 $MGLOPT $MGLINC"

if [ $# -lt 2 ]; then
	echo "Usage: bash quick_relink_main.sh <build_version> <file> [more files...]" >&2
	exit 1
fi

QUAKE3_BUILD_VERSION="$1"
shift

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

in_main() { case " $(echo $MAINFILES) " in *" $1 "*) return 0;; esac; return 1; }

for arg in "$@"; do
	rel=${arg#../../}   # the old code/-relative spelling of a driver file
	case "$rel" in
		MiniGLV3D/*) src="$V3D/$rel" ;;
		*)           src="$ROOT/code/$rel" ;;
	esac
	if [ ! -f "$src" ]; then
		echo "ERROR: $src does not exist" >&2
		exit 1
	fi

	file=$(basename "$src")
	case "$rel" in
		amiga/snd_int68k_gas.s) obj=snd_int68k ;;
		*)                      obj=${file%.*} ;;
	esac
	if ! in_main "$obj"; then
		echo "ERROR: $rel builds $obj.o, which is not linked into the main executable" >&2
		exit 1
	fi

	# Working directory and source spelling as the full build has them: it
	# compiles from the file's own directory by basename, except dll.c and the
	# sound assembler, which it names by full path from code/amiga.
	cdir=$(dirname "$src")
	srcarg=$file
	case "$rel" in
		amiga/dll/dll.c|amiga/snd_int68k_gas.s) cdir="$ROOT/code/amiga"; srcarg=$src ;;
	esac

	echo "=== recompile ($rel) ==="
	case "$rel" in
		amiga/snd_int68k_gas.s)
			( cd "$cdir" && $AS -m68020 -m68881 "$srcarg" -o "$OUT/$obj.o" ) ;;
		botlib/be_ai_char.c)
			( cd "$cdir" && $GCC $O0FLAGS -c "$srcarg" -o "$OUT/$obj.o" ) ;;
		MiniGLV3D/backend/hw/v3d_debug.c)
			( cd "$cdir" && $GCC $MGLBASE -DDEBUG -c "$srcarg" -o "$OUT/$obj.o" ) ;;
		MiniGLV3D/*)
			( cd "$cdir" && $GCC $MGLBASE -c "$srcarg" -o "$OUT/$obj.o" ) ;;
		renderer/*|amiga/*)
			( cd "$cdir" && $GCC $CFLAGS $MGLINC -c "$srcarg" -o "$OUT/$obj.o" ) ;;
		*)
			( cd "$cdir" && $GCC $CFLAGS -c "$srcarg" -o "$OUT/$obj.o" ) ;;
	esac
done

MAINOBJS=""; for f in $MAINFILES; do MAINOBJS="$MAINOBJS $OUT/$f.o"; done

echo "=== RELINK: quake3-68k-v${QUAKE3_BUILD_VERSION} (main only) ==="
$GCC -mcpu=68020 -m68881 -mcrt=clib2 -o "$BUILDOUT/quake3-68k-v${QUAKE3_BUILD_VERSION}" \
  $MAINOBJS -L"$LIBM020" -lm881 -lm -lc -lamiga
echo "LINK exit=$? -- output: $BUILDOUT/quake3-68k-v${QUAKE3_BUILD_VERSION}"
