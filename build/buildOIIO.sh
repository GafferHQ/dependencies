#!/bin/sh

set -e

cd `dirname $0`/../oiio-Release-1.5.13

rm -f CMakeCache.txt

cmake \
	-D CMAKE_INSTALL_PREFIX=$BUILD_DIR \
	-D CMAKE_PREFIX_PATH=$BUILD_DIR \
	.

make && make install
