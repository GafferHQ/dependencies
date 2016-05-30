#!/bin/bash

set -e

cd `dirname $0`/../Python-2.7.5

mkdir -p $BUILD_DIR/doc/licenses
cp LICENSE $BUILD_DIR/doc/licenses/python

if [[ `uname` = "Linux" ]] ; then

	export LD_LIBRARY_PATH=$BUILD_DIR/lib
	./configure --prefix=$BUILD_DIR --enable-shared --enable-unicode=ucs4
	make clean && make && make install

else

	export DYLD_LIBRARY_PATH=$BUILD_DIR/lib	
	./configure --prefix=$BUILD_DIR --enable-framework=$BUILD_DIR/lib --enable-unicode=ucs4
	make clean && make && make install
	
	cd $BUILD_DIR/bin && ln -fsh ../lib/Python.framework/Versions/Current/bin/python python

fi
