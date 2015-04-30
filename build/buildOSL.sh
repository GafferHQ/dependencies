#!/bin/sh

set -e

cd `dirname $0`/../OpenShadingLanguage-Release-1.6.3dev

# needed because the build process runs oslc, which
# needs to link to the oiio libraries
export DYLD_LIBRARY_PATH=$BUILD_DIR/lib
export LD_LIBRARY_PATH=$BUILD_DIR/lib

mkdir -p gafferBuild
cd gafferBuild

rm -f CMakeCache.txt

cmake \
	-D ENABLERTTI=1 \
	-D CMAKE_INSTALL_PREFIX=$BUILD_DIR \
	-D CMAKE_PREFIX_PATH=$BUILD_DIR \
	-D STOP_ON_WARNING=0 \
	..

make && make install
