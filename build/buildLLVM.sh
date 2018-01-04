#!/bin/bash

set -e

cd `dirname $0`/../llvm-3.4

# if both clang and gcc available, and CXX isn't specified, configure
# seems to prefer clang. that fails to build on os x, so we force
# the use of gcc.
export CXX=g++
export CC=gcc

./configure --prefix=$BUILD_DIR --enable-shared --enable-optimized --enable-assertions=no
env REQUIRES_RTTI=1 make VERBOSE=1 -j `getconf _NPROCESSORS_ONLN` && make install
