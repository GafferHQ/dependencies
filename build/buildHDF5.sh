#!/bin/bash

set -e

cd `dirname $0`/../hdf5-1.8.11

mkdir -p $BUILD_DIR/doc/licenses
cp COPYING $BUILD_DIR/doc/licenses/hdf5

./configure --prefix=$BUILD_DIR --enable-threadsafe --with-pthread=/usr/include && make clean && make -j 4 && make install
