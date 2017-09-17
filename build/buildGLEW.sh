#!/bin/bash

set -e

cd `dirname $0`/../glew-2.1.0

mkdir -p $BUILD_DIR/doc/licenses
cp LICENSE.txt $BUILD_DIR/doc/licenses/glew

mkdir -p $BUILD_DIR/lib64/pkgconfig
make clean && make -j `getconf _NPROCESSORS_ONLN` install GLEW_DEST=$BUILD_DIR LIBDIR=$BUILD_DIR/lib
