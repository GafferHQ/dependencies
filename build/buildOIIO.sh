#!/bin/bash

set -e

cd `dirname $0`/../oiio-Release-1.5.17

mkdir -p $BUILD_DIR/doc/licenses
cp LICENSE $BUILD_DIR/doc/licenses/openImageIO

mkdir -p gafferBuild
cd gafferBuild

rm -f CMakeCache.txt

cmake \
	-D CMAKE_INSTALL_PREFIX=$BUILD_DIR \
	-D CMAKE_PREFIX_PATH=$BUILD_DIR \
	-D USE_FFMPEG=OFF \
	..

make && make install
