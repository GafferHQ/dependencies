#!/bin/sh

pushd `dirname $0`/../oiio-Release-1.5.13
cmake -DCMAKE_INSTALL_PREFIX=$BUILD_DIR && make && make install
popd
