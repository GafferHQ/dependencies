#!/bin/sh

pushd `dirname $0`/../libpng-1.6.3
./configure --prefix=$BUILD_DIR && make clean && make && make install
popd
