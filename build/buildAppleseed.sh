#!/bin/bash

set -e

appleseedVersion="1.8.1-beta"

# Make clean working directory, and change into it

workingDir=`dirname $0`/../working/appleseed
rm -rf $workingDir
mkdir -p $workingDir
cd $workingDir

# Unpack the source archive

archive=appleseed-$appleseedVersion.tar.gz
cp ../../archives/$archive ./
tar -xf $archive

cd appleseed-$appleseedVersion

# Needed so that `oslc` can be run to compile
# shaders during the build.
export LD_LIBRARY_PATH=$BUILD_DIR/lib
export DYLD_FALLBACK_LIBRARY_PATH=$BUILD_DIR/lib

# Appleseed embeds minizip, which appears to require a later version
# of zlib than CentOS 6 provides. These defines disable encryption,
# which isn't needed anyway, and fixes the problem.
# See https://github.com/appleseedhq/appleseed/issues/1597.
export CFLAGS="-DNOCRYPT -DNOUNCRYPT"

# Make sure we pick up the python headers from $BUILD_DIR,
# rather than any system level headers.
if [[ `uname` = "Linux" ]] ; then
	pythonIncludeDir=$BUILD_DIR/include/python2.7
else
	pythonIncludeDir=$BUILD_DIR/lib/Python.framework/Headers
fi

# I don't know what the sandbox is or why things are copied there
# when we're installing somewhere else, but if the directories
# don't exist, the install fails. It seems that the directories do
# exist in the appleseed repository, but the process of downloading
# a tarball and committing them to the gafferDependencies
# repository loses it. It's easier to fix that here than to remember
# every time.
mkdir -p sandbox/bin
mkdir -p sandbox/schemas

mkdir -p build
cd build

cmake \
	-D WITH_CLI=ON \
	-D WITH_STUDIO=OFF \
	-D WITH_TOOLS=OFF \
	-D WITH_TESTS=OFF \
	-D WITH_PYTHON=ON \
	-D USE_STATIC_BOOST=OFF \
	-D USE_STATIC_OIIO=OFF \
	-D USE_STATIC_OSL=OFF \
	-D USE_EXTERNAL_ZLIB=ON \
	-D USE_EXTERNAL_EXR=ON \
	-D USE_EXTERNAL_PNG=ON \
	-D USE_EXTERNAL_XERCES=ON \
	-D USE_EXTERNAL_OSL=ON \
	-D USE_EXTERNAL_OIIO=ON \
	-D USE_SSE=ON \
	-D WARNINGS_AS_ERRORS=OFF \
	-D CMAKE_PREFIX_PATH=$BUILD_DIR \
	-D CMAKE_INSTALL_PREFIX=$BUILD_DIR/appleseed \
	-D PYTHON_INCLUDE_DIR=$pythonIncludeDir \
	..

make clean
make -j 4 VERBOSE=1
make install
