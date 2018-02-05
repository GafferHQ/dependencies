#!/bin/bash

set -e

cd `dirname $0`/../qt-adsk-5.6.1

mkdir -p $BUILD_DIR/doc/licenses
cp LICENSE.LGPLv21 $BUILD_DIR/doc/licenses/qt

export LD_LIBRARY_PATH=$BUILD_DIR/lib

if [[ `uname` = "Darwin" ]] ; then
	extraArgs=-no-freetype
else
	extraArgs=-qt-xcb
fi

./configure \
	-prefix $BUILD_DIR \
	-plugindir $BUILD_DIR/qt/plugins \
	-release \
	-opensource -confirm-license \
	-no-rpath -no-gtkstyle \
	-no-audio-backend -no-dbus \
	-skip qtconnectivity \
	-skip qtwebengine \
	-skip qt3d \
	-skip qtdeclarative \
	-no-libudev \
	-no-icu \
	-qt-pcre \
	-nomake examples \
	-nomake tests \
	$extraArgs \
	-I $BUILD_DIR/include -I $BUILD_DIR/include/freetype2 \
	-L $BUILD_DIR/lib

make -j `getconf _NPROCESSORS_ONLN` && make install
