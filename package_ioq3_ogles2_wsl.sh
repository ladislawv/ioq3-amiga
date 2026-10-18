#!/bin/bash
# Packs the OpenGL ES 2.0 build (build_ioq3_ogles2_wsl.sh) for users:
# build_ogles2/Quake3-GLES2-<version>.lha, holding the drawer Quake3-GLES2/:
#   quake3-gl2, Quake3-GLES2 (a start script), baseq3/*.dll
#   libs/ogles2.library, libs/v3dglslc     (ogles2: library and compiler)
#   shaders/*.v3dp, *.v3ds                 (the renderer's programs, compiled)
#   ReadMe-GLES2.txt, COPYING.txt, licences/
# In Linux or WSL, after the build:
#
#   OGLES2_RELEASE=/path/to/ogles2 SHADERS=/path/to/shaders bash package_ioq3_ogles2_wsl.sh [VERSION]
#
# OGLES2_RELEASE: an unpacked ogles2 user archive (ogles2-1.3.lha from
# github.com/ladislawv/ogles2/releases: Libs/, LICENSE, licences/).
# OGLES2_LIB: a different ogles2.library to ship (default: the release's).
# SHADERS: the renderer's compiled programs and shader records (.v3dp,
# .v3ds), as ogles2 writes them into OGLES2Cache: when the game first runs
# in a screen mode (ogles2's tests/a1200/m6-5/cache holds 640x480 and
# 1920x1080). LHA: the lha program (default ~/tools/bin/lha, else lha).
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
VERSION=${1:-296}
REL=${OGLES2_RELEASE:?set OGLES2_RELEASE to an unpacked ogles2 user archive}
SH=${SHADERS:?set SHADERS to the drawer of compiled shaders}
LIB=${OGLES2_LIB:-$REL/Libs/ogles2.library}
LHA=${LHA:-$HOME/tools/bin/lha}
[ -x "$LHA" ] || LHA=lha
OUT=$HERE/build_ogles2
P=$OUT/pkg/Quake3-GLES2
[ -f "$OUT/quake3-gl2" ] || { echo "build first (build_ioq3_ogles2_wsl.sh)"; exit 1; }
[ -f "$LIB" ] && [ -f "$REL/Libs/v3dglslc" ] && [ -f "$REL/LICENSE" ] || { echo "$REL is not an unpacked ogles2 user archive"; exit 1; }
ls "$SH"/*.v3dp > /dev/null 2>&1 || { echo "no .v3dp files in $SH"; exit 1; }

rm -rf "$OUT/pkg"
mkdir -p "$P/baseq3" "$P/libs" "$P/shaders" "$P/licences/ogles2"
cp "$OUT/quake3-gl2" "$P/"
cp "$OUT"/cgame68k.dll "$OUT"/qagame68k.dll "$OUT"/ui68k.dll "$P/baseq3/"
cp "$LIB" "$P/libs/ogles2.library"
cp "$REL/Libs/v3dglslc" "$P/libs/"
cp "$SH"/*.v3dp "$P/shaders/"
cp "$SH"/*.v3ds "$P/shaders/" 2>/dev/null || true
# Texts with Amiga line ends.
sed 's/\r$//' "$HERE/code/amiga-gl2/ReadMe-GLES2.txt" > "$P/ReadMe-GLES2.txt"
sed 's/\r$//' "$HERE/code/amiga-gl2/Quake3-GLES2" > "$P/Quake3-GLES2"
sed 's/\r$//' "$HERE/COPYING.txt" > "$P/COPYING.txt"
cp "$REL/LICENSE" "$P/licences/ogles2/LICENSE"
cp -r "$REL/licences/." "$P/licences/ogles2/"

(cd "$OUT/pkg" && rm -f "../Quake3-GLES2-$VERSION.lha" && $LHA -ao5q "../Quake3-GLES2-$VERSION.lha" Quake3-GLES2)
ls -l "$OUT/Quake3-GLES2-$VERSION.lha"
(cd "$OUT" && sha256sum "Quake3-GLES2-$VERSION.lha")
echo "$(ls "$P/shaders" | wc -l) files in shaders/"
