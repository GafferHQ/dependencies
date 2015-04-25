#!/bin/sh

set -e

for n in "BUILD_DIR" "ARNOLD_ROOT" "RMAN_ROOT" "NOTES" ; do
	if [ -z "${!n}" ]; then
		echo "ERROR : $n environment variable not set"
		exit 1
	fi
done

cd `dirname $0`
buildPython.sh
buildSubprocess32.sh
buildBoost.sh
boostJPEG.sh
buildTIFF.sh
buildPNG.sh
buildFreeType.sh
buildTBB.sh
buildEXR.sh
buildFonts.sh
buildGLEW.sh
buildOCIO.sh
buildOIIO.sh
buildLLVM.sh
buildOSL.sh
buildHDF5.sh
buildAlembic.sh
buildCortex.sh
buildPyOpenGL.sh
buildQt.sh
buildPySide.sh

