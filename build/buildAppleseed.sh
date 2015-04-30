#!/bin/sh

set -e

cd `dirname $0`/../appleseed-d3f996b

mkdir -p build
cd build
rm -f CMakeCache.txt

cmake \
	-D WITH_CLI=ON \
	-D WITH_STUDIO=OFF \
	-D WITH_TOOLS=OFF \
	-D WITH_PYTHON=ON \
	-D WITH_OSL=ON \
	-D USE_STATIC_BOOST=OFF \
	-D USE_STATIC_OIIO=OFF \
	-D USE_STATIC_OSL=OFF \
	-D USE_EXTERNAL_ZLIB=ON \
	-D USE_EXTERNAL_EXR=ON \
	-D USE_EXTERNAL_PNG=ON \
	-D USE_EXTERNAL_XERCES=ON \
	-D USE_EXTERNAL_OSL=ON \
	-D USE_EXTERNAL_OIIO=ON \
	-D USE_EXTERNAL_ALEMBIC=ON \
	-D CMAKE_PREFIX_PATH=$BUILD_DIR \
	-D CMAKE_INSTALL_PREFIX=$BUILD_DIR/appleseed \
	..

make clean
make -j 4
make install
