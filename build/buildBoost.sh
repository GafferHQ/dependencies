#!/bin/bash

set -e

cd `dirname $0`/../boost_1_61_0

mkdir -p $BUILD_DIR/doc/licenses
cp LICENSE_1_0.txt $BUILD_DIR/doc/licenses/boost

# without this, boost build will still pick up the system python framework,
# even though we tell it quite explicitly to use the one in $BUILD_DIR.
export DYLD_FALLBACK_FRAMEWORK_PATH=$BUILD_DIR/lib

export MACOSX_DEPLOYMENT_TARGET=10.9

# give a helping hand to find the python headers, since the bootstrap below doesn't
# always seem to get it right.
export CPLUS_INCLUDE_PATH=$BUILD_DIR/include/python2.7

./bootstrap.sh --prefix=$BUILD_DIR --with-python=$BUILD_DIR/bin/python --with-python-root=$BUILD_DIR --without-libraries=log
./bjam -d+2 -j 4 cxxflags="-std=c++11" variant=release link=shared threading=multi install
