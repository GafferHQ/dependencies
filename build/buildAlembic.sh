#!/bin/bash

set -e

cd `dirname $0`/../alembic-1.5.8

mkdir -p $BUILD_DIR/doc/licenses
cp LICENSE.txt $BUILD_DIR/doc/licenses/alembic

# may need to hand edit build/AlembicBoost.cmake in the alembic distribution to remove Boost_USE_STATIC_LIBS.
# may also need to set ALEMBIC_NO_TESTS=TRUE on OSX (in CMakeLists.txt).

rm -f CMakeCache.txt
cmake \
	-D CMAKE_INSTALL_PREFIX=$BUILD_DIR \
	-D CMAKE_PREFIX_PATH=$BUILD_DIR \
	-D Boost_NO_SYSTEM_PATHS=TRUE \
	-D Boost_NO_BOOST_CMAKE=TRUE \
	-D BOOST_ROOT=$BUILD_DIR \
	-D ILMBASE_ROOT=$BUILD_DIR \
	-D USE_PYILMBASE=FALSE \
	-D USE_PYALEMBIC=FALSE \
	-D USE_ARNOLD=FALSE \
	-D USE_PRMAN=FALSE \
	-D USE_MAYA=FALSE \
	.

make clean
make
make install

rm -rf $BUILD_DIR/include/Alembic
mv $BUILD_DIR/alembic-*/include/Alembic $BUILD_DIR/include
mv $BUILD_DIR/alembic-*/lib/static/* $BUILD_DIR/lib
