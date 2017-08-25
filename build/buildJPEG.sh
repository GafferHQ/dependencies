#!/bin/bash

set -e

cd `dirname $0`/../libjpeg-turbo-1.5.2

mkdir -p $BUILD_DIR/doc/licenses
cp LICENSE.md $BUILD_DIR/doc/licenses/libjpeg-turbo

./configure --prefix=$BUILD_DIR && make clean && make && make install
