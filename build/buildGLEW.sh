#!/bin/bash

set -e

GLEW_VERSION="2.1.0"
ARCHIVE_NAME="glew-$GLEW_VERSION.tgz"
ARCHIVE_ROOT_DIR="glew-$GLEW_VERSION"

WORKING_DIR=`dirname $0`/../working/glew

rm -rf $WORKING_DIR
mkdir -p $WORKING_DIR
cp `dirname $0`/../archives/$ARCHIVE_NAME  $WORKING_DIR

pushd $WORKING_DIR

tar xvf ${ARCHIVE_NAME}
cd $ARCHIVE_ROOT_DIR

mkdir -p $BUILD_DIR/doc/licenses
cp LICENSE.txt $BUILD_DIR/doc/licenses/glew

mkdir -p $BUILD_DIR/lib64/pkgconfig
make clean && make -j `getconf _NPROCESSORS_ONLN` install GLEW_DEST=$BUILD_DIR LIBDIR=$BUILD_DIR/lib

popd
