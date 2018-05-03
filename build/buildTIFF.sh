#!/bin/bash

set -e

cd `dirname $0`/../tiff-4.0.8

# needed to make sure we link against the libjpeg
# in the gaffer distribution and not the system
# libjpeg.
export CPPFLAGS=-I$BUILD_DIR/include
export LDFLAGS=-L$BUILD_DIR/lib

mkdir -p $BUILD_DIR/doc/licenses
cp COPYRIGHT $BUILD_DIR/doc/licenses/libtiff

./configure --without-x --prefix=$BUILD_DIR
make clean && make -j `getconf _NPROCESSORS_ONLN` && make install
