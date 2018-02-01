#!/bin/bash

set -e

cd `dirname $0`/../oiio-Release-1.7.15

mkdir -p $BUILD_DIR/doc/licenses
cp LICENSE $BUILD_DIR/doc/licenses/openImageIO

mkdir -p gafferBuild
cd gafferBuild

rm -f CMakeCache.txt

cmake \
	-D CMAKE_INSTALL_PREFIX=$BUILD_DIR \
	-D CMAKE_PREFIX_PATH=$BUILD_DIR \
	-D USE_FFMPEG=OFF \
	-D USE_QT=OFF \
	-D USE_CPP11=1 \
	..

make -j `getconf _NPROCESSORS_ONLN` VERBOSE=1 && make install
