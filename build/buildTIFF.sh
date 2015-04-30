#!/bin/sh

set -e

cd `dirname $0`/../tiff-3.8.2
./configure --prefix=$BUILD_DIR && make clean && make && make install
