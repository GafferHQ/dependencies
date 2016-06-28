#!/bin/bash

set -e

cd `dirname $0`/../openssl-1.0.2h

mkdir -p $BUILD_DIR/doc/licenses
cp LICENSE $BUILD_DIR/doc/licenses/openssl

if [[ `uname` = "Linux" ]] ; then

	./config --prefix=$BUILD_DIR -fPIC
	make
	make install

else

	export KERNEL_BITS=64
	./config --prefix=$BUILD_DIR -fPIC
	make
	make install

fi