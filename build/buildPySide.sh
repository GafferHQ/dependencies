#!/bin/sh

set -e

PYTHON_VERSION=2.7
export PATH=$BUILD_DIR/bin:$PATH

pushd `dirname $0`/../shiboken-1.2.2

rm -rf build && mkdir build && cd build

if [[ `uname` = "Linux" ]] ; then
	extraArgs="-DPYTHON_INCLUDE_DIR=$BUILD_DIR/include/python$PYTHON_VERSION"
	export LD_LIBRARY_PATH=$BUILD_DIR/lib
else
	# OS X
	extraArgs="-DPYTHON_INCLUDE_DIR=$BUILD_DIR/lib/Python.framework/Headers -DPYTHON_LIBRARY=$BUILD_DIR/Python.framework/Versions/$PYTHON_VERSION/libpython${PYTHON_VERSION}.dylib"
	export DYLD_FALLBACK_FRAMEWORK_PATH=$BUILD_DIR/lib
fi

cmake .. \
	-DCMAKE_BUILD_TYPE=Release \
	-DPYTHON_SITE_PACKAGES=$BUILD_DIR/python \
	-DCMAKE_INSTALL_PREFIX=$BUILD_DIR \
	-DPYTHON_EXECUTABLE=$BUILD_DIR/bin/python \
	-DCMAKE_PREFIX_PATH=$BUILD_DIR \
	$extraArgs

make clean && make VERBOSE=1 -j 4 && make install
	
popd

cd `dirname $0`/../pyside-qt4.8+1.2.2

rm -rf build && mkdir build && cd build
cmake \
	-D CMAKE_BUILD_TYPE=Release \
	-D SITE_PACKAGE=$BUILD_DIR/python \
	-D CMAKE_INSTALL_PREFIX=$BUILD_DIR \
	-D ALTERNATIVE_QT_INCLUDE_DIR=$BUILD_DIR/include \
	..

make clean && make VERBOSE=1 -j 4 && make install
