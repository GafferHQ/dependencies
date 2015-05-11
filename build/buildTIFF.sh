#!/bin/sh

set -e

cd `dirname $0`/../tiff-3.8.2

mkdir -p $BUILD_DIR/doc/licenses
cp COPYRIGHT $BUILD_DIR/doc/licenses/libtiff

./configure --prefix=$BUILD_DIR && make clean && make && make install
