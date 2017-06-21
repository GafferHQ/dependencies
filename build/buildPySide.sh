#!/bin/bash

set -e

cd `dirname $0`/../pyside-setup-6d8dee0

export LD_LIBRARY_PATH=$BUILD_DIR/lib
export PATH=$BUILD_DIR/bin:$PATH
export MACOSX_DEPLOYMENT_TARGET=10.9

python setup.py --ignore-git --osx-use-libc++ install
