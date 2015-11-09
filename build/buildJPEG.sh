#!/bin/bash

set -e

cd `dirname $0`/../jpeg-8c

mkdir -p $BUILD_DIR/doc/licenses
cp README $BUILD_DIR/doc/licenses/libjpeg

./configure --prefix=$BUILD_DIR && make clean && make && make install
