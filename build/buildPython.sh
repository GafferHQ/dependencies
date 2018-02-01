#!/bin/bash

set -e

cd `dirname $0`/../Python-2.7.13

mkdir -p $BUILD_DIR/doc/licenses
cp LICENSE $BUILD_DIR/doc/licenses/python

if [[ `uname` = "Linux" ]] ; then

	./configure --prefix=$BUILD_DIR --enable-shared --enable-unicode=ucs4
	make clean && make -j `getconf _NPROCESSORS_ONLN` && make install

else

	./configure --prefix=$BUILD_DIR --enable-framework=$BUILD_DIR/lib --enable-unicode=ucs4
	make clean && make -j `getconf _NPROCESSORS_ONLN` && make install

	# Install will have made symlinks to the absolute location of
	# the python binaries inside the framework. Make them relative.

	cd $BUILD_DIR/bin
	for f in python*; do
		ln -fsh ../lib/Python.framework/Versions/Current/bin/$f $f
	done

fi
