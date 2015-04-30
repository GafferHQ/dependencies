#!/bin/sh

set -e

cd `dirname $0`/../freetype-2.4.12
./configure --prefix=$BUILD_DIR && make clean && make && make install
