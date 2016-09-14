#!/bin/bash

set -e

cd `dirname $0`/../tbb43_20150611oss

mkdir -p $BUILD_DIR/doc/licenses
cp COPYING $BUILD_DIR/doc/licenses/tbb

if [[ -z $CXX ]] ; then
	# tbb will prefer gcc over clang unless explicitly told otherwise,
	# but fails to build with clang 2.1 on OS X 10.7.
	export CXX=gcc
fi

# If our default shell is not Bash, TBB's own build system
# may not be able to properly figure out which OS we're on,
# so set it manually here.
if [[ `uname` = "Linux" ]] ; then
    export tbb_os=linux
else
    export tbb_os=macos
fi

make clean && make compiler=$CXX

cp -r include/tbb $BUILD_DIR/include

if [[ `uname` = "Linux" ]] ; then

	cp build/*_release/*.so* $BUILD_DIR/lib

else

	cp build/macos_*_release/*.dylib $BUILD_DIR/lib

fi
