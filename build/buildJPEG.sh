#!/bin/sh

set -e

cd `dirname $0`/../jpeg-8c
./configure --prefix=$BUILD_DIR && make clean && make && make install
