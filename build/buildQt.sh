#!/bin/sh

set -e

cd `dirname $0`/../qt-everywhere-opensource-src-4.8.5

./configure \
	-prefix $BUILD_DIR \
	-opensource -confirm-license \
	-no-rpath -no-declarative -no-gtkstyle -no-qt3support \
	-no-multimedia -no-audio-backend -no-webkit -no-script -no-dbus -no-declarative -no-svg \
	-nomake examples -nomake demos -nomake tools \
	-I $BUILD_DIR/include -L $BUILD_DIR/lib

make -j 4 && make install
