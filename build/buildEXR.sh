#!/bin/sh

pushd `dirname $0`/../ilmbase-2.1.0
./configure --prefix=$BUILD_DIR && make clean && make && make install
popd

pushd `dirname $0`/../openexr-2.1.0
./configure --prefix=$BUILD_DIR && make clean && make && make install
popd
