#!/bin/bash

set -e

cd `dirname $0`/../PyOpenGL-3.0.2

export LD_LIBRARY_PATH=$BUILD_DIR/lib
export DYLD_FRAMEWORK_PATH=$BUILD_DIR/lib

$BUILD_DIR/bin/python setup.py install --prefix $BUILD_DIR --install-lib $BUILD_DIR/python
