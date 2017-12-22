#!/bin/bash

set -e

pushd `dirname $0`/../ilmbase-2.2.0

mkdir -p $BUILD_DIR/doc/licenses
cp COPYING $BUILD_DIR/doc/licenses/ilmbase

./configure --prefix=$BUILD_DIR && make clean && make && make install

popd

export PATH=$BUILD_DIR/bin:$PATH
export LD_LIBRARY_PATH=$BUILD_DIR/lib
export DYLD_FALLBACK_LIBRARY_PATH=$BUILD_DIR/lib

pushd `dirname $0`/../pyilmbase-2.2.0

mkdir -p $BUILD_DIR/doc/licenses
cp COPYING $BUILD_DIR/doc/licenses/pyilmbase

./configure --prefix=$BUILD_DIR --with-boost-include-dir=$BUILD_DIR/include --without-numpy && make clean && make && make install

mkdir -p $BUILD_DIR/python
mv $BUILD_DIR/lib/python*/site-packages/iexmodule.so $BUILD_DIR/python
mv $BUILD_DIR/lib/python*/site-packages/imathmodule.so $BUILD_DIR/python

popd

pushd `dirname $0`/../openexr-2.2.0

cp LICENSE $BUILD_DIR/doc/licenses/openexr

./configure --prefix=$BUILD_DIR && make clean && make && make install

popd
