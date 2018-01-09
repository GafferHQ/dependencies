#!/bin/bash

set -e

usdVersion="0.8.2"

workingDir=`dirname $0`/../working/usd
rm -rf $workingDir
mkdir -p $workingDir
cd $workingDir

archive=USD-$usdVersion.tar.gz
cp ../../archives/$archive ./
tar -xf $archive

cd USD-$usdVersion

cp LICENSE.txt $BUILD_DIR/doc/licenses/usd

export LD_LIBRARY_PATH=$BUILD_DIR/lib

cmake \
	-D CMAKE_PREFIX_PATH=$BUILD_DIR \
	-D CMAKE_INSTALL_PREFIX=$BUILD_DIR \
	-D Boost_NO_BOOST_CMAKE=TRUE \
	-D PXR_BUILD_IMAGING=FALSE \
	-D PXR_BUILD_TESTS=FALSE \
	.

make install -j `getconf _NPROCESSORS_ONLN` VERBOSE=1

mv $BUILD_DIR/lib/python/pxr $BUILD_DIR/python
