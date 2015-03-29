#!/bin/sh

pushd `dirname $0`/../boost_1_51_0

./bootstrap.sh --prefix=$BUILD_DIR --with-python=$BUILD_DIR/bin/python --with-python-root=$BUILD_DIR
./bjam -d+2 variant=release link=shared threading=multi install

popd
