#!/bin/sh

cd `dirname $0`/../glew-1.7.0
mkdir -p $BUILD_DIR/lib64/pkgconfig
make clean && make install GLEW_DEST=$BUILD_DIR LIBDIR=$BUILD_DIR/lib
