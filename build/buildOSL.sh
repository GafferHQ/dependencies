#!/bin/bash

set -e

cd `dirname $0`/../OpenShadingLanguage-Release-1.7.5
mkdir -p $BUILD_DIR/doc/licenses
cp LICENSE $BUILD_DIR/doc/licenses/osl

# needed because the build process runs oslc, which
# needs to link to the oiio libraries
export DYLD_FALLBACK_LIBRARY_PATH=$BUILD_DIR/lib
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
