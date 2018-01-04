#!/bin/bash

set -e

jpegVersion="1.5.2"

# Make clean working directory, and change into it

workingDir=`dirname $0`/../working/jpeg
rm -rf $workingDir
mkdir -p $workingDir
cd $workingDir

# Unpack the source archive

archive=libjpeg-turbo-$jpegVersion.tar.gz
cp ../../archives/$archive ./
tar -xf $archive

cd libjpeg-turbo-$jpegVersion

# Copy license over

mkdir -p $BUILD_DIR/doc/licenses
cp LICENSE.md $BUILD_DIR/doc/licenses/libjpeg-turbo

# Build and install

./configure --prefix=$BUILD_DIR
make -j `getconf _NPROCESSORS_ONLN`
make install
