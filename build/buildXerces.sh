#!/bin/bash

set -e

cd `dirname $0`/../xerces-c-3.1.2

mkdir -p $BUILD_DIR/doc/licenses
cp LICENSE $BUILD_DIR/doc/licenses/xerces

./configure --prefix=$BUILD_DIR
make -j `getconf _NPROCESSORS_ONLN`
make install
