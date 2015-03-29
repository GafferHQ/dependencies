#!/bin/sh

pushd `dirname $0`/../boost_1_51_0

# without this, boost build will still pick up the system python framework,
# even though we tell it quite explicitly to use the one in $BUILD_DIR.
export DYLD_FALLBACK_FRAMEWORK_PATH=$BUILD_DIR/lib

./bootstrap.sh --prefix=$BUILD_DIR --with-python=$BUILD_DIR/bin/python --with-python-root=$BUILD_DIR
./bjam -d+2 variant=release link=shared threading=multi install

popd
