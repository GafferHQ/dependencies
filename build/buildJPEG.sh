#!/bin/sh

pushd `dirname $0`/../jpeg-8c
./configure --prefix=$BUILD_DIR && make clean && make && make install
popd
