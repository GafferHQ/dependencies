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
./buildCMark.sh
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
(cd .. && ./build/build.py --project OpenImageIO --buildDir $BUILD_DIR)
./buildBlosc.sh
./buildVDB.sh
(cd .. && ./build/build.py --project LLVM --buildDir $BUILD_DIR)
(cd .. && ./build/build.py --project OpenShadingLanguage --buildDir $BUILD_DIR)
./buildHDF5.sh
./buildAlembic.sh
(cd .. && ./build/build.py --project Xerces --buildDir $BUILD_DIR)
(cd .. && ./build/build.py --project Appleseed --buildDir $BUILD_DIR)
(cd .. && ./build/build.py --project GafferResources --buildDir $BUILD_DIR)
./buildUSD.sh
./buildCortex.sh
./buildPyOpenGL.sh
./buildQt.sh
./buildPySide.sh
./buildQtPy.sh
./buildPackage.sh
