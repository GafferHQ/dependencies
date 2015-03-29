#!/bin/sh

pushd `dirname $0`/../Alembic_1.5.0_2013072300

# may need to hand edit build/AlembicBoost.cmake in the alembic distribution to remove Boost_USE_STATIC_LIBS.
# may also need to set ALEMBIC_NO_TESTS=TRUE on OSX (in CMakeLists.txt).

rm -f CMakeCache.txt
cmake \
	-DCMAKE_INSTALL_PREFIX=$BUILD_DIR \
	-DBoost_NO_SYSTEM_PATHS=TRUE \
	-DBoost_NO_BOOST_CMAKE=TRUE \
	-DBOOST_ROOT=$BUILD_DIR \
	-DILMBASE_ROOT=$BUILD_DIR \
	-DUSE_PYILMBASE=FALSE \
	-DUSE_PYALEMBIC=FALSE

make clean && make -j 4 && make install

mv $BUILD_DIR/alembic-*/include/* $BUILD_DIR/include
mv $BUILD_DIR/alembic-*/lib/static/* $BUILD_DIR/lib

popd
