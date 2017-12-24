#!/bin/bash

set -e
shopt -s nullglob

VERSION=0.42.0.0

PYTHON_VERSION=2.7

if [[ `uname` = "Linux" ]] ; then
	SHLIBSUFFIX=".so"
	PLATFORM="linux"
else
	SHLIBSUFFIX=".dylib"
	PLATFORM="osx"
fi

cd $BUILD_DIR

manifest="

	bin/moc
	bin/qmake
	bin/rcc
	bin/uic

	bin/python
	bin/python*[0-9]

	bin/exrheader
	bin/maketx
	bin/oiiotool
	bin/oslc
	bin/oslinfo

	bin/abcconvert
	bin/abcecho
	bin/abcechobounds
	bin/abcls
	bin/abcstitcher
	bin/abctree

	lib/libboost_*$SHLIBSUFFIX*
	lib/libboost_test_exec_monitor.a

	lib/libIECore*$SHLIBSUFFIX

	lib/libIex*$SHLIBSUFFIX*
	lib/libHalf*$SHLIBSUFFIX*
	lib/libImath*$SHLIBSUFFIX*
	lib/libIlmImf*$SHLIBSUFFIX*
	lib/libIlmThread*$SHLIBSUFFIX*

	lib/libPyIex*$SHLIBSUFFIX*
	lib/libPyImath*$SHLIBSUFFIX*

	lib/libtiff*$SHLIBSUFFIX*
	lib/libfreetype*$SHLIBSUFFIX*
	lib/libjpeg*$SHLIBSUFFIX*
	lib/libpng*$SHLIBSUFFIX*

	lib/libOpenImageIO*$SHLIBSUFFIX*
	lib/libOpenColorIO*$SHLIBSUFFIX*

	lib/libLLVM*$SHLIBSUFFIX*
	lib/libosl*

	lib/libpython*$SHLIBSUFFIX*
	lib/Python.framework*
	lib/python$PYTHON_VERSION

	lib/libGLEW*$SHLIBSUFFIX*
	lib/libtbb*$SHLIBSUFFIX*

	lib/libhdf5*$SHLIBSUFFIX*
	lib/libAlembic*

	lib/libQt*
	lib/Qt*.framework

	lib/libxerces-c*$SHLIBSUFFIX*

	lib/libopenvdb*$SHLIBSUFFIX*
	lib/libblosc*$SHLIBSUFFIX*

	fonts
	resources
	shaders
	qt

	openColorIO

	glsl/IECoreGL
	glsl/*.frag
	glsl/*.vert

	doc/licenses
	doc/cortex/html
	doc/osl*

	python/IECore*
	python/9to10
	python/OpenGL
	python/PyOpenColorIO*
	python/Qt.py
	python/pyopenvdb*
	python/iexmodule*
	python/imathmodule*

	include/IECore*
	include/boost
	include/GL
	include/OpenEXR
	include/python*
	include/tbb
	include/OSL
	include/OpenImageIO
	include/OpenColorIO
	include/Qt*
	include/freetype2
	include/Alembic
	include/openvdb
	include/tiff*
	include/png*
	include/libpng*
	include/jconfig.h
	include/jerror.h
	include/jmorecfg.h
	include/jpeglib.h
	include/pyopenvdb.h

	renderMan
	arnold

	appleseedDisplays

	appleseed/bin/appleseed.cli
	appleseed/include
	appleseed/lib
	appleseed/samples
	appleseed/schemas
	appleseed/settings
	appleseed/shaders

"

packageName=gafferDependencies-$VERSION-$PLATFORM
archiveName=$packageName.tar.gz

# Longwinded method for putting a prefix on the filenames
# in the archive - there is an option for this in GNU tar
# but that's not available on OS X.

tar -c -z -f /tmp/intermediate.tar $manifest
rm -rf /tmp/$packageName
mkdir /tmp/$packageName
cd /tmp/$packageName
tar -x -f /tmp/intermediate.tar
cd /tmp
tar -c -z -f `dirname $BUILD_DIR`/$archiveName $packageName
