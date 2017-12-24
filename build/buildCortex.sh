#!/bin/bash

set -e

cortexVersion=10.0.0-a8

workingDir=`dirname $0`/../working/cortex
rm -rf $workingDir
mkdir -p $workingDir
cd $workingDir

archive=cortex-$cortexVersion.tar.gz
cp ../../archives/$archive ./
tar -xf $archive
cd cortex-$cortexVersion

mkdir -p $BUILD_DIR/doc/licenses
cp LICENSE $BUILD_DIR/doc/licenses/cortex

export LD_LIBRARY_PATH=$BUILD_DIR/lib

rm -rf .sconsign.dblite .sconf_temp

scons install installDoc \
	-j 3 \
	CXX=`which g++` \
	INSTALL_PREFIX=$BUILD_DIR \
	INSTALL_DOC_DIR=$BUILD_DIR/doc/cortex \
	INSTALL_RMANPROCEDURAL_NAME=$BUILD_DIR/renderMan/procedurals/iePython \
	INSTALL_RMANDISPLAY_NAME=$BUILD_DIR/renderMan/displayDrivers/ieDisplay \
	INSTALL_PYTHON_DIR=$BUILD_DIR/python \
	INSTALL_ARNOLDPROCEDURAL_NAME=/tmp/unusedGafferDependencies/ieProcedural.so \
	INSTALL_ARNOLDOUTPUTDRIVER_NAME=$BUILD_DIR/arnold/plugins/ieOutputDriver.so \
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

cp -r contrib/scripts/9to10 $BUILD_DIR/python
