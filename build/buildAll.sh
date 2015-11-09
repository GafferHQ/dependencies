#!/bin/bash

set -e

for n in "BUILD_DIR" "VERSION" "ARNOLD_ROOT" "RMAN_ROOT" ; do
	if [ -z "${!n}" ]; then
		echo "ERROR : $n environment variable not set"
		exit 1
	fi
done

cd `dirname $0`
./buildPython.sh
./buildSubprocess32.sh
./buildBoost.sh
./buildJPEG.sh
./buildTIFF.sh
./buildPNG.sh
./buildFreeType.sh
./buildTBB.sh
./buildEXR.sh
./buildFonts.sh
./buildGLEW.sh
./buildOCIO.sh
./buildOIIO.sh
./buildLLVM.sh
./buildOSL.sh
./buildHDF5.sh
./buildAlembic.sh
./buildXerces.sh
./buildAppleseed.sh
./buildCortex.sh
./buildPyOpenGL.sh
./buildQt.sh
./buildPySide.sh
./buildPackage.sh
