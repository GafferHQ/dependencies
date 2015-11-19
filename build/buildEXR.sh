#!/bin/bash

set -e

pushd `dirname $0`/../ilmbase-2.2.0

mkdir -p $BUILD_DIR/doc/licenses
cp COPYING $BUILD_DIR/doc/licenses/ilmbase

./configure --prefix=$BUILD_DIR && make clean && make && make install

popd

export LD_LIBRARY_PATH=$BUILD_DIR/lib

pushd `dirname $0`/../openexr-2.2.0

cp LICENSE $BUILD_DIR/doc/licenses/openexr

./configure --prefix=$BUILD_DIR && make clean && make && make install

popd
