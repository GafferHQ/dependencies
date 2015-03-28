#!/bin/sh

pushd `dirname $0`/../xerces-c-3.1.2
pwd
./configure --prefix=$BUILD_DIR
make
make install
popd
