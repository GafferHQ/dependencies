#!/bin/bash

set -e

mkdir -p $BUILD_DIR/resources/cortex
cp `dirname $0`/../tileset_2048.dat $BUILD_DIR/resources/cortex

cd `dirname $0`/../cortex-9.0.0

mkdir -p $BUILD_DIR/doc/licenses
cp LICENSE $BUILD_DIR/doc/licenses/cortex

export LD_LIBRARY_PATH=$BUILD_DIR/lib

rm -rf .sconsign.dblite .sconf_temp

scons install installDoc \
	-j 3 \
	INSTALL_PREFIX=$BUILD_DIR \
	INSTALL_DOC_DIR=$BUILD_DIR/doc/cortex \
	INSTALL_RMANPROCEDURAL_NAME=$BUILD_DIR/renderMan/procedurals/iePython \
	INSTALL_RMANDISPLAY_NAME=$BUILD_DIR/renderMan/displayDrivers/ieDisplay \
	INSTALL_PYTHON_DIR=$BUILD_DIR/python \
	INSTALL_ARNOLDPROCEDURAL_NAME=$BUILD_DIR/arnold/procedurals/ieProcedural.so \
	INSTALL_ARNOLDOUTPUTDRIVER_NAME=$BUILD_DIR/arnold/outputDrivers/ieOutputDriver.so \
	INSTALL_IECORE_OPS='' \
	PYTHON_CONFIG=$BUILD_DIR/bin/python-config \
	BOOST_INCLUDE_PATH=$BUILD_DIR/include/boost \
	LIBPATH=$BUILD_DIR/lib \
	BOOST_LIB_SUFFIX='' \
	OPENEXR_INCLUDE_PATH=$BUILD_DIR/include \
	FREETYPE_INCLUDE_PATH=$BUILD_DIR/include/freetype2 \
	WITH_GL=1 \
	GLEW_INCLUDE_PATH=$BUILD_DIR/include/GL \
	RMAN_ROOT=$RMAN_ROOT \
	NUKE_ROOT= \
	ARNOLD_ROOT=$ARNOLD_ROOT \
	APPLESEED_ROOT=$BUILD_DIR/appleseed \
	APPLESEED_INCLUDE_PATH=$BUILD_DIR/appleseed/include \
	APPLESEED_LIB_PATH=$BUILD_DIR/appleseed/lib \
	ENV_VARS_TO_IMPORT=LD_LIBRARY_PATH \
	OPTIONS='' \
	SAVE_OPTIONS=gaffer.options
