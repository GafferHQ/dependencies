#!/bin/sh

cd `dirname $0`/../subprocess32-3.2.6

# ensure that we use the right python to do the install
export PATH=$BUILD_DIR/bin:$PATH
export DYLD_FRAMEWORK_PATH=$BUILD_DIR/lib
export LD_LIBRARY_PATH=$BUILD_DIR/lib

python setup.py install

