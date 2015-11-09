#!/bin/bash

set -e

cd `dirname $0`/../ttf-bitstream-vera-1.10

mkdir -p $BUILD_DIR/fonts
cp *.ttf $BUILD_DIR/fonts
