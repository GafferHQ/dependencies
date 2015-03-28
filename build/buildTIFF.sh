#!/bin/sh

pushd `dirname $0`/../tiff-3.8.2
./configure --prefix=$BUILD_DIR && make clean && make && make install
popd
