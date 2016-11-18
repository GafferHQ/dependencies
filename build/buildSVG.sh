#!/bin/bash

set -e

cd `dirname $0`/../libsvg-0.1.4

mkdir -p $BUILD_DIR/doc/licenses
cp COPYING $BUILD_DIR/doc/licenses/libsvg

export PKG_CONFIG_PATH=$BUILD_DIR/lib/pkgconfig
./configure --enable-shared --prefix=$BUILD_DIR && make clean && make && make install
