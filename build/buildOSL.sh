#!/bin/bash

set -e

oslVersion="1.8.12"

# Make clean working directory, and change into it

workingDir=`dirname $0`/../working/osl
rm -rf $workingDir
mkdir -p $workingDir
cd $workingDir

# Unpack the source archive, and apply patches

archive=OpenShadingLanguage-Release-$oslVersion.tar.gz
cp ../../archives/$archive ./
tar -xf $archive

cd OpenShadingLanguage-Release-$oslVersion

patch -p1 < ../../../patches/osl/llvmCompileFlagsMac.patch

# Build!

mkdir -p $BUILD_DIR/doc/licenses
cp LICENSE $BUILD_DIR/doc/licenses/osl

# needed because the build process runs oslc, which
# needs to link to the oiio libraries
export DYLD_FALLBACK_LIBRARY_PATH=$BUILD_DIR/lib
export LD_LIBRARY_PATH=$BUILD_DIR/lib

if [[ `uname` = "Linux" ]] ; then
	# OSL compiles llvm_ops.cpp using clang rather
	# than g++. We need to make sure that clang uses
	# the same headers as the main compiler. This is
	# essential when building for C++11 on a system
	# where the default GCC does not have C++11
	# compatible headers.
	gccRoot=$(which g++ | sed s/bin\\/g++//)
	llvmCompileFlags="--gcc-toolchain=$gccRoot"
fi

mkdir -p gafferBuild
cd gafferBuild

rm -f CMakeCache.txt

cmake \
	-D ENABLERTTI=1 \
	-D CMAKE_INSTALL_PREFIX=$BUILD_DIR \
	-D CMAKE_PREFIX_PATH=$BUILD_DIR \
	-D STOP_ON_WARNING=0 \
	-D USE_CPP11=1 \
	-D LLVM_COMPILE_FLAGS=$llvmCompileFlags \
	..

make VERBOSE=1 && make install
