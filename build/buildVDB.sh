#!/bin/bash

set -e

cd `dirname $0`/../openvdb-4.0.2/openvdb

mkdir -p $BUILD_DIR/doc/licenses
cp LICENSE $BUILD_DIR/doc/licenses/openvdb

PYTHON_VERSION=2.7

if [[ `uname` = "Linux" ]] ; then
	PYTHON_INCL_DIR=$BUILD_DIR/include/python$PYTHON_VERSION
	PYTHON_LIB_DIR=$BUILD_DIR/lib
else
	# OS X
	PYTHON_INCL_DIR=$BUILD_DIR/lib/Python.framework/Headers
	PYTHON_LIB_DIR=$BUILD_DIR/lib/Python.framework/Versions/$PYTHON_VERSION/
fi

make clean
make install \
	DESTDIR=$BUILD_DIR \
	BOOST_INCL_DIR=$BUILD_DIR/include \
	BOOST_LIB_DIR=$BUILD_DIR/lib \
	BOOST_PYTHON_LIB_DIR=$BUILD_DIR/lib \
	BOOST_PYTHON_LIB=-lboost_python \
	EXR_INCL_DIR=$BUILD_DIR/include \
	EXR_LIB_DIR=$BUILD_DIR/lib \
	TBB_INCL_DIR=$BUILD_DIR/include \
	TBB_LIB_DIR=$BUILD_DIR/lib \
	PYTHON_VERSION=$PYTHON_VERSION \
	PYTHON_INCL_DIR=$PYTHON_INCL_DIR \
	PYTHON_LIB_DIR=$PYTHON_LIB_DIR \
	BLOSC_INCL_DIR=$BUILD_DIR/include \
	BLOSC_LIB_DIR=$BUILD_DIR/lib \
	NUMPY_INCL_DIR= \
	CONCURRENT_MALLOC_LIB= \
	GLFW_INCL_DIR= \
	LOG4CPLUS_INCL_DIR= \
	EPYDOC=

mv $BUILD_DIR/python/lib/python$PYTHON_VERSION/pyopenvdb.so $BUILD_DIR/python
mv $BUILD_DIR/python/include/python2.7/pyopenvdb.h $BUILD_DIR/include
