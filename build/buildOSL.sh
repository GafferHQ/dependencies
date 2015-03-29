#!/bin/sh

pushd `dirname $0`/../OpenShadingLanguage-Release-1.6.3dev

# needed because the build process runs oslc, which
# needs to link to the oiio libraries
export DYLD_LIBRARY_PATH=$BUILD_DIR/lib
export LD_LIBRARY_PATH=$BUILD_DIR/lib

mkdir -p gafferBuild
cd gafferBuild

cmake -DENABLERTTI=1 -DCMAKE_INSTALL_PREFIX=$BUILD_DIR ..
make && make install

popd
