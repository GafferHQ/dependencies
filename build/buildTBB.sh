#!/bin/bash

set -e

tbbVersion=2017_U6

workingDir=`dirname $0`/../working/tbb
rm -rf $workingDir
mkdir -p $workingDir
cd $workingDir

archive=tbb-$tbbVersion.tar.gz
cp ../../archives/$archive ./
tar -xf $archive
cd tbb-$tbbVersion

mkdir -p $BUILD_DIR/doc/licenses
cp LICENSE $BUILD_DIR/doc/licenses/tbb

# If our default shell is not Bash, TBB's own build system
# may not be able to properly figure out which OS we're on,
# so set it manually here.
if [[ `uname` = "Linux" ]] ; then
    export tbb_os=linux
else
    export tbb_os=macos
fi

make -j `getconf _NPROCESSORS_ONLN` stdver=c++11

cp -r include/tbb $BUILD_DIR/include

if [[ `uname` = "Linux" ]] ; then

	cp build/*_release/*.so* $BUILD_DIR/lib

else

	cp build/macos_*_release/*.dylib $BUILD_DIR/lib

fi
