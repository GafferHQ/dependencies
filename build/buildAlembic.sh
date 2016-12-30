#!/bin/bash

set -e

cd `dirname $0`/../alembic-1.6.1

mkdir -p $BUILD_DIR/doc/licenses
cp LICENSE.txt $BUILD_DIR/doc/licenses/alembic

rm -f CMakeCache.txt
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
	-D ALEMBIC_LIB_USES_BOOST=TRUE \
	.

make clean
make
make install
