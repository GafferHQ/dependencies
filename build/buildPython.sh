#!/bin/sh

set -e

cd `dirname $0`/../Python-2.7.5

if [[ `uname` = "Linux" ]] ; then

	./configure --prefix=$BUILD_DIR --enable-shared --enable-unicode=ucs4
	make clean && make && make install

else
	
	./configure --prefix=$BUILD_DIR --enable-framework=$BUILD_DIR/lib --enable-unicode=ucs4
	make clean && make && make install
	
	cd $BUILD_DIR/bin && ln -fsh ../lib/Python.framework/Versions/Current/bin/python python

fi
