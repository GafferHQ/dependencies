#!/bin/bash

set -e

pushd `dirname $0`/../OpenColorIO-1.0.9

export LD_LIBRARY_PATH=$BUILD_DIR/lib

mkdir -p $BUILD_DIR/doc/licenses
cp LICENSE $BUILD_DIR/doc/licenses/openColorIO

cmake -DCMAKE_INSTALL_PREFIX=$BUILD_DIR -DPYTHON=$BUILD_DIR/bin/python -DOCIO_BUILD_TRUELIGHT=OFF -DOCIO_BUILD_APPS=OFF -DOCIO_BUILD_NUKE=OFF
make clean && make -j 4 && make install

mkdir -p $BUILD_DIR/python
mv $BUILD_DIR/lib/python*/site-packages/PyOpenColorIO* $BUILD_DIR/python

popd

mkdir -p $BUILD_DIR/openColorIO
cp `dirname $0`/../imageworks-OpenColorIO-Configs-f931d77/nuke-default/config.ocio $BUILD_DIR/openColorIO
cp -r `dirname $0`/../imageworks-OpenColorIO-Configs-f931d77/nuke-default/luts $BUILD_DIR/openColorIO
