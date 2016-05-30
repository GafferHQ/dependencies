#!/bin/bash

set -e

cd `dirname $0`/../libxml2-2.9.4

mkdir -p $BUILD_DIR/doc/licenses
cp COPYING $BUILD_DIR/doc/licenses/libxml2

./configure --without-python --prefix=$BUILD_DIR && make clean && make && make install
