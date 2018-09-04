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
(cd .. && ./build/build.py --project Python --buildDir $BUILD_DIR)
./buildCMark.sh
./buildSubprocess32.sh
(cd .. && ./build/build.py --project Boost --buildDir $BUILD_DIR)
./buildJPEG.sh
./buildTIFF.sh
(cd .. && ./build/build.py --project PNG --buildDir $BUILD_DIR)
(cd .. && ./build/build.py --project FreeType --buildDir $BUILD_DIR)
./buildTBB.sh
./buildEXR.sh
./buildFonts.sh
(cd .. && ./build/build.py --project GLEW --buildDir $BUILD_DIR)
(cd .. && ./build/build.py --project OpenColorIO --buildDir $BUILD_DIR)
(cd .. && ./build/build.py --project OpenImageIO --buildDir $BUILD_DIR)
(cd .. && ./build/build.py --project Blosc --buildDir $BUILD_DIR)
./buildVDB.sh
(cd .. && ./build/build.py --project LLVM --buildDir $BUILD_DIR)
(cd .. && ./build/build.py --project OpenShadingLanguage --buildDir $BUILD_DIR)
(cd .. && ./build/build.py --project HDF5 --buildDir $BUILD_DIR)
(cd .. && ./build/build.py --project Alembic --buildDir $BUILD_DIR)
(cd .. && ./build/build.py --project Xerces --buildDir $BUILD_DIR)
(cd .. && ./build/build.py --project Appleseed --buildDir $BUILD_DIR)
(cd .. && ./build/build.py --project GafferResources --buildDir $BUILD_DIR)
./buildUSD.sh
(cd .. && ./build/build.py --project Cortex --buildDir $BUILD_DIR)
./buildPyOpenGL.sh
./buildQt.sh
./buildPySide.sh
./buildQtPy.sh
./buildPackage.sh
