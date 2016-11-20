#!/bin/bash

set -e

cd `dirname $0`/../qt-everywhere-opensource-src-4.8.5

mkdir -p $BUILD_DIR/doc/licenses
cp LICENSE.LGPL $BUILD_DIR/doc/licenses/qt

export LD_LIBRARY_PATH=$BUILD_DIR/lib

./configure \
	-prefix $BUILD_DIR \
	-opensource -confirm-license \
	-no-rpath -no-declarative -no-gtkstyle -no-qt3support \
	-no-multimedia -no-audio-backend -no-script -no-dbus -no-svg \
	-nomake examples -nomake demos -nomake tools \
	-I $BUILD_DIR/include -L $BUILD_DIR/lib

make -j 4 && make install
