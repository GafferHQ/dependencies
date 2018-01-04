#!/bin/bash

set -e

pushd `dirname $0`/../OpenColorIO-1.0.9

export LD_LIBRARY_PATH=$BUILD_DIR/lib

mkdir -p $BUILD_DIR/doc/licenses
cp LICENSE $BUILD_DIR/doc/licenses/openColorIO

# By default, OCIO will use tr1::shared_ptr. But Maya (2015 and 2016 at least)
# ships with a libOpenColorIO built using boost::shared_ptr instead. We'd like
# Gaffer's default packages to be useable in Maya, so we pass OCIO_USE_BOOST_PTR=1
# to match Maya's build. Even though both Gaffer and Maya respect the VFXPlatform
# 2016 by using OCIO 1.0.9, this is an example of where the platform is under
# specified, and we must go the extra mile to get compatibility.
cmake -DCMAKE_INSTALL_PREFIX=$BUILD_DIR -DPYTHON=$BUILD_DIR/bin/python -DOCIO_USE_BOOST_PTR=1 -DOCIO_BUILD_TRUELIGHT=OFF -DOCIO_BUILD_APPS=OFF -DOCIO_BUILD_NUKE=OFF
make clean && make -j `getconf _NPROCESSORS_ONLN` && make install

mkdir -p $BUILD_DIR/python
mv $BUILD_DIR/lib/python*/site-packages/PyOpenColorIO* $BUILD_DIR/python

popd

mkdir -p $BUILD_DIR/openColorIO
cp `dirname $0`/../imageworks-OpenColorIO-Configs-f931d77/nuke-default/config.ocio $BUILD_DIR/openColorIO
cp -r `dirname $0`/../imageworks-OpenColorIO-Configs-f931d77/nuke-default/luts $BUILD_DIR/openColorIO
