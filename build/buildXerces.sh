#!/bin/sh

set -e

cd `dirname $0`/../xerces-c-3.1.2

./configure --prefix=$BUILD_DIR
make
make install
