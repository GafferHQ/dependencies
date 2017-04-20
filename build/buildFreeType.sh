#!/bin/bash

set -e

cd `dirname $0`/../freetype-2.7.1

mkdir -p $BUILD_DIR/doc/licenses
cp docs/FTL.TXT $BUILD_DIR/doc/licenses/freetype

./configure --without-png --prefix=$BUILD_DIR && make clean && make && make install
