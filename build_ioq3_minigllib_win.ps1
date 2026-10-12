# ioq3 as a minigl.library CLIENT, Windows-native build (2026-10-08).
# Port of build_ioq3_minigllib_wsl.sh to this machine: same GCC 6.5.0b + clib2
# toolchain (C:\amiga-gcc), same flags and file lists, MiniGL headers and
# import library from the 29.1 client SDK (C:\MiniGL_Library_Quake3\SDK).
#
# MAIN EXECUTABLE ONLY -- cgame/qagame/ui touch no GL and no sound, build
# them with the WSL showcase script when their sources change.
#
# Full fresh recompile every run (never a stale relink). Run from anywhere:
#   powershell -ExecutionPolicy Bypass -File build_ioq3_minigllib_win.ps1
$ErrorActionPreference = 'Stop'

$GCC  = 'C:\amiga-gcc\bin\m68k-amigaos-gcc.exe'
$AS   = 'C:\amiga-gcc\bin\m68k-amigaos-as.exe'
$LIBM020 = 'C:\amiga-gcc\m68k-amigaos\clib2\lib\libm020'
# cc1.exe needs the msys-*.dll runtime that lives next to the drivers
$env:PATH = "C:\amiga-gcc\bin;$env:PATH"

$ROOT = $PSScriptRoot
$SDK  = 'C:\MiniGL_Library_Quake3\SDK'
# client-facing headers FIRST -- they are what a library client compiles against
$MGLINC = @("-I$SDK\minigl-shared-library\include", "-I$SDK\backend\include", "-I$ROOT\code\amiga")

$OUT      = "$ROOT\ioquake3_objs_68k_minigllib"
$BUILDOUT = "$ROOT\build_output_lib"
New-Item -ItemType Directory -Force $OUT, $BUILDOUT | Out-Null

$COMMON = @('-std=c99', '-mcpu=68020', '-m68881', '-mcrt=clib2',
	'-fno-strict-aliasing', '-fno-builtin-cos', '-fno-builtin-sin',
	'-DAMIGAOS', '-DNDEBUG', '-DBOTLIB', '-DUSE_LOCAL_HEADERS', '-DDLL', '-DNO_VM_COMPILED', '-DUSE_MINIGL_LIBRARY')
$CFLAGS  = @('-O2', '-finline-functions') + $COMMON
# be_ai_char.c is hardware-confirmed to need -O0 (bots-not-spawning bisection, 2026-08-29)
$O0FLAGS = @('-O0') + $COMMON

function Compile($dir, $files, $flags, $extra = @()) {
	foreach ($f in $files) {
		Write-Host "=== $dir ($f.c) ==="
		& $GCC @flags @extra -c "$ROOT\code\$dir\$f.c" -o "$OUT\$f.o"
		if ($LASTEXITCODE -ne 0) { throw "compile failed: $dir\$f.c" }
	}
}

$ZLIB_SRC  = 'adler32 crc32 inffast inflate inftrees zutil' -split ' '
$JPEG      = ('jaricom jcapimin jcapistd jcarith jccoefct jccolor jcdctmgr jchuff jcinit jcmainct ' +
	'jcmarker jcmaster jcomapi jcparam jcprepct jcsample jctrans jdapimin jdapistd jdarith ' +
	'jdatadst jdatasrc jdcoefct jdcolor jddctmgr jdhuff jdinput jdmainct jdmarker jdmaster ' +
	'jdmerge jdpostct jdsample jdtrans jerror jfdctflt jfdctfst jfdctint jidctflt jidctfst ' +
	'jidctint jmemmgr jmemnobs jquant1 jquant2 jutils') -split ' '
$QCOMMON   = ('cm_load cm_patch cm_polylib cm_test cm_trace cmd common cvar files huffman md4 msg ' +
	'net_chan net_ip q_math q_shared1 q_shared2 q_shared3 q_shared4 q_shared5 q_shared6 ' +
	'q_shared7 q_shared8 md5 puff unzip ioapi vm vm_interpreted') -split ' '
$SERVER    = 'sv_bot sv_ccmds sv_client sv_game sv_init sv_main sv_net_chan sv_snapshot sv_world' -split ' '
$BOTLIB    = ('be_aas_bspq3 be_aas_cluster be_aas_debug be_aas_entity be_aas_file be_aas_main ' +
	'be_aas_move be_aas_optimize be_aas_reach be_aas_route be_aas_routealt be_aas_sample ' +
	'be_ai_chat be_ai_gen be_ai_goal be_ai_move be_ai_weap be_ai_weight be_ea be_interface ' +
	'l_crc l_libvar l_log l_memory l_precomp l_script l_struct') -split ' '
$RENDERER  = ('tr_animation tr_backend tr_bsp tr_cmds tr_curve tr_flares tr_font tr_image tr_init ' +
	'tr_light tr_main tr_marks tr_mesh tr_model tr_noise tr_scene tr_shade tr_shade_calc ' +
	'tr_shader tr_shadows tr_sky tr_surface tr_world tr_image_bmp tr_image_tga tr_image_jpg ' +
	'tr_image_png tr_image_pcx') -split ' '
$CLIENT    = ('cl_cgame cl_cin cl_console cl_input cl_keys cl_main cl_net_chan cl_parse cl_scrn ' +
	'cl_ui cl_avi snd_adpcm snd_dma snd_mem snd_mix snd_wavelet snd_main snd_codec snd_codec_wav') -split ' '
$AMIGA     = 'amiga_main amiga_input amiga_mhandler amiga_snd amiga_snd_ahi amiga_glimp amiga_shared mglv3d_log_noop' -split ' '

Compile 'zlib'     $ZLIB_SRC $CFLAGS
Compile 'jpeg-8c'  $JPEG     $CFLAGS
Compile 'qcommon'  $QCOMMON  $CFLAGS
Compile 'server'   $SERVER   $CFLAGS
Compile 'botlib'   $BOTLIB   $CFLAGS
Compile 'botlib'   @('be_ai_char') $O0FLAGS
Compile 'renderer' $RENDERER $CFLAGS $MGLINC
Compile 'client'   $CLIENT   $CFLAGS
Compile 'amiga'    $AMIGA    $CFLAGS $MGLINC
Compile 'amiga\dll' @('dll') $CFLAGS $MGLINC

Write-Host '=== amiga asm (GAS): snd_int68k_gas.s ==='
& $AS -m68020 -m68881 "$ROOT\code\amiga\snd_int68k_gas.s" -o "$OUT\snd_int68k.o"
if ($LASTEXITCODE -ne 0) { throw 'assemble failed: snd_int68k_gas.s' }

# Derived from the source marker, never hardcoded (see the WSL scripts' history).
$m = Select-String -Path "$ROOT\code\amiga\amiga_glimp.c" -Pattern '#define QUAKE3_BUILD_VERSION (\d+)'
if (-not $m) { throw 'could not read QUAKE3_BUILD_VERSION from code/amiga/amiga_glimp.c' }
$VER = $m.Matches[0].Groups[1].Value

$objs = ($ZLIB_SRC + $JPEG + $QCOMMON + $SERVER + $BOTLIB + 'be_ai_char' + $RENDERER + $CLIENT + $AMIGA + 'dll' + 'snd_int68k') |
	ForEach-Object { "$OUT\$_.o" }

# No -lnet: its startup constructor hard-fails without bsdsocket.library
# (2026-08-27); net_ip.c reimplements everything it needed.
$EXE = "$BUILDOUT\quake3-68k-minigllib-v$VER"
Write-Host "=== LINK: quake3-68k-minigllib-v$VER (main, library client) ==="
& $GCC -mcpu=68020 -m68881 -mcrt=clib2 -o $EXE @objs "-L$SDK\lib" -lminigl "-L$LIBM020" -lm881 -lm -lc -lamiga
if ($LASTEXITCODE -ne 0) { throw 'link failed' }
Write-Host "=== built: $EXE ==="
