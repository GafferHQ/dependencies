#!/bin/bash

set -e

BLOSC_VERSION="1.7.0"
ARCHIVE_NAME="c-blosc-$BLOSC_VERSION.tar.gz"
ARCHIVE_ROOT_DIR="c-blosc-$BLOSC_VERSION"

WORKING_DIR=`dirname $0`/../working/blosc

rm -rf $WORKING_DIR
mkdir -p $WORKING_DIR
cp `dirname $0`/../archives/$ARCHIVE_NAME  $WORKING_DIR

pushd $WORKING_DIR

tar xvf ${ARCHIVE_NAME}
cd $ARCHIVE_ROOT_DIR

mkdir -p $BUILD_DIR/doc/licenses/blosc
cp LICENSES/* $BUILD_DIR/doc/licenses/blosc/

cmake -DCMAKE_INSTALL_PREFIX=$BUILD_DIR .
make install VERBOSE=1

popd
