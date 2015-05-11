#!/bin/sh

set -e

cd `dirname $0`/../freetype-2.4.12

mkdir -p $BUILD_DIR/doc/licenses
cp docs/FTL.TXT $BUILD_DIR/doc/licenses/freetype

./configure --prefix=$BUILD_DIR && make clean && make && make install
