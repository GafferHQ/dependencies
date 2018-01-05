#!/bin/bash

set -e

alembicVersion=1.7.5

workingDir=`dirname $0`/../working/alembic
rm -rf $workingDir
mkdir -p $workingDir
cd $workingDir

archive=alembic-$alembicVersion.tar.gz
cp ../../archives/$archive ./
tar -xf $archive
cd alembic-$alembicVersion

mkdir -p $BUILD_DIR/doc/licenses
cp LICENSE.txt $BUILD_DIR/doc/licenses/alembic

cmake \
	-D CMAKE_INSTALL_PREFIX=$BUILD_DIR \
	-D CMAKE_PREFIX_PATH=$BUILD_DIR \
	-D Boost_NO_SYSTEM_PATHS=TRUE \
	-D Boost_NO_BOOST_CMAKE=TRUE \
	-D BOOST_ROOT=$BUILD_DIR \
	-D ILMBASE_ROOT=$BUILD_DIR \
	-D HDF5_ROOT=$BUILD_DIR \
	-D USE_HDF5=TRUE \
	-D USE_PYILMBASE=FALSE \
	-D USE_PYALEMBIC=FALSE \
	-D USE_ARNOLD=FALSE \
	-D USE_PRMAN=FALSE \
	-D USE_MAYA=FALSE \
	.

make clean
make VERBOSE=1 -j `getconf _NPROCESSORS_ONLN`
make install
