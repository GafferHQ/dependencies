#!/bin/bash

set -e

cd `dirname $0`/../tiff-3.8.2

# needed to make sure we link against the libjpeg
# in the gaffer distribution and not the system
# libjpeg.
export CPPFLAGS=-I$BUILD_DIR/include
export LDFLAGS=-L$BUILD_DIR/lib

mkdir -p $BUILD_DIR/doc/licenses
cp COPYRIGHT $BUILD_DIR/doc/licenses/libtiff

./configure --prefix=$BUILD_DIR && make clean && make && make install
