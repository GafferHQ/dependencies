#!/bin/sh

pushd `dirname $0`/../hdf5-1.8.11
./configure --prefix=$BUILD_DIR --enable-threadsafe --with-pthread=/usr/include && make clean && make -j 4 && make install
popd
