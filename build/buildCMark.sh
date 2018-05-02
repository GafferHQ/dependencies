#!/bin/bash

set -e

cmarkVersion=0.28.3.gfm.12

workingDir=`dirname $0`/../working/cmark
rm -rf $workingDir
mkdir -p $workingDir
cd $workingDir

archive=cmark-$cmarkVersion.tar.gz
cp ../../archives/$archive ./
tar -xf $archive
cd cmark-$cmarkVersion

mkdir -p $BUILD_DIR/doc/licenses
cp COPYING $BUILD_DIR/doc/licenses/cmark

mkdir build
cd build
cmake .. -DCMAKE_INSTALL_PREFIX=$BUILD_DIR
make
make install
