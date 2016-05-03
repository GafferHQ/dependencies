#!/bin/sh

set -e

cd `dirname $0`/../openvdb-3.1.0/openvdb

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
	NUMPY_INCL_DIR= \
	CONCURRENT_MALLOC_LIB= \
	GLFW_INCL_DIR= \
	LOG4CPLUS_INCL_DIR= \
	EPYDOC=
