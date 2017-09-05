#!/bin/bash

set -e

cd `dirname $0`/../libjpeg-turbo-1.5.2

mkdir -p $BUILD_DIR/doc/licenses
cp LICENSE.md $BUILD_DIR/doc/licenses/libjpeg-turbo

# Note :
# You may need to run `autoreconf -ivf` if git checks out the files
# with timestamps that convince autotools that things are not up to
# date and need rerunning. Really we need to switch things so we
# download tarballs rather than keep the code in git.

./configure --prefix=$BUILD_DIR && make clean && make && make install
