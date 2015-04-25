#!/bin/sh

set -e

cd `dirname $0`/../cortex-9.0.0-b4

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
	RMAN_ROOT=$DELIGHT \
	WITH_GL=1 \
	GLEW_INCLUDE_PATH=$BUILD_DIR/include/GL \
	RMAN_ROOT=$RMAN_ROOT \
	NUKE_ROOT= \
	ARNOLD_ROOT=$ARNOLD_ROOT \
	OPTIONS='' \
	SAVE_OPTIONS=gaffer.options
