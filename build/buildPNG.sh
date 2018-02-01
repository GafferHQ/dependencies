#!/bin/bash

set -e

cd `dirname $0`/../libpng-1.6.3

mkdir -p $BUILD_DIR/doc/licenses
cp LICENSE $BUILD_DIR/doc/licenses/libpng

./configure --prefix=$BUILD_DIR && make clean && make -j `getconf _NPROCESSORS_ONLN` && make install
