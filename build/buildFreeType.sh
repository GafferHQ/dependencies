#!/bin/bash

set -e

cd `dirname $0`/../freetype-2.7.1

mkdir -p $BUILD_DIR/doc/licenses
cp docs/FTL.TXT $BUILD_DIR/doc/licenses/freetype

export PATH=$BUILD_DIR/bin:$PATH
export LDFLAGS=-L$BUILD_DIR/lib

./configure --prefix=$BUILD_DIR --with-harfbuzz=no
make clean && make -j `getconf _NPROCESSORS_ONLN` && make install
