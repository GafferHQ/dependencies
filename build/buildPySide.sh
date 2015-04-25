#!/bin/sh

set -e

set PYTHON_VERSION 2.7

pushd `dirname $0`/../shiboken-1.2.2

rm -rf build && mkdir build && cd build

if [[ `uname` = "Linux" ]] ; then
	set extraArgs="-DPYTHON_INCLUDE_DIR=$BUILD_DIR/include/python$PYTHON_VERSION"
else
	# OS X
	set extraArgs="-DPYTHON_INCLUDE_DIR=$BUILD_DIR/lib/Python.framework/Headers -DPYTHON_LIBRARY=$BUILD_DIR/Python.framework/Versions/$PYTHON_VERSION/libpython${PYTHON_VERSION}.dylib"
	export DYLD_FALLBACK_FRAMEWORK_PATH=$BUILD_DIR/lib
fi

cmake .. \
	-DCMAKE_BUILD_TYPE=Release \
	-DPYTHON_SITE_PACKAGES=$BUILD_DIR/python \
	-DCMAKE_INSTALL_PREFIX=$BUILD_DIR \
	-DPYTHON_EXECUTABLE=$BUILD_DIR/bin/python \
	-DCMAKE_PREFIX_PATH=$BUILD_DIR \
	$extraArgs

make clean && make -j 4 && make install
	
popd

cd `dirname $0`/../pyside-qt4.8+1.2.2

rm -rf build && mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DSITE_PACKAGE=$BUILD_DIR/python -DCMAKE_INSTALL_PREFIX=$BUILD_DIR -DALTERNATIVE_QT_INCLUDE_DIR=$BUILD_DIR/include

make clean && make -j 4 && make install
