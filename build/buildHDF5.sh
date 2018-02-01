#!/bin/bash

set -e

hdf5Version=1.8.20

workingDir=`dirname $0`/../working/hdf5
rm -rf $workingDir
mkdir -p $workingDir
cd $workingDir

archive=hdf5-$hdf5Version.tar.gz
cp ../../archives/$archive ./
tar -xf $archive
cd hdf5-$hdf5Version

mkdir -p $BUILD_DIR/doc/licenses
cp COPYING $BUILD_DIR/doc/licenses/hdf5

./configure --prefix=$BUILD_DIR --enable-threadsafe --disable-hl --with-pthread=/usr/include
make -j `getconf _NPROCESSORS_ONLN`
make install
