#!/bin/bash

set -e

for n in "BUILD_DIR" "ARNOLD_ROOT" "RMAN_ROOT" ; do
	if [ -z "${!n}" ]; then
		echo "ERROR : $n environment variable not set"
		exit 1
	fi
done

if [[ `uname` = "Darwin" ]] ; then
	if csrutil status | grep -q enabled ; then
		# SIP prevents DYLD_* environment variables
		# being inherited by protected processes, which
		# breaks the builds for OSL, PySide and Appleseed.
		# These each try to run a binary they create during
		# the build, and cmake/make run that binary via a
		# shell which throws away the library path, preventing
		# runtime linking from working.
		echo "ERROR : SIP not disabled"
		exit 1
	fi
fi

cd `dirname $0`
./buildOpenSSL.sh
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
./buildBlosc.sh
./buildVDB.sh
./buildLLVM.sh
./buildOSL.sh
./buildHDF5.sh
./buildAlembic.sh
./buildXerces.sh
./buildAppleseed.sh
./buildResources.sh
./buildCortex.sh
./buildPyOpenGL.sh
./buildQt.sh
./buildPySide.sh
./buildQtPy.sh
./buildPackage.sh
