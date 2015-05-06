#!/bin/sh

set -e
shopt -s nullglob

if [[ `uname` = "Linux" ]] ; then
	SHLIBSUFFIX=".so"
	PLATFORM="linux"
else
	SHLIBSUFFIX=".dylib"
	PLATFORM="osx"
fi

cd $BUILD_DIR

manifest="

	bin/python
	bin/python*[0-9]

	bin/maketx
	bin/oslc
	bin/oslinfo

	lib/libboost_signals*$SHLIBSUFFIX*
	lib/libboost_thread*$SHLIBSUFFIX*
	lib/libboost_wave*$SHLIBSUFFIX*
	lib/libboost_regex*$SHLIBSUFFIX*
	lib/libboost_python*$SHLIBSUFFIX*
	lib/libboost_date_time*$SHLIBSUFFIX*
	lib/libboost_filesystem*$SHLIBSUFFIX*
	lib/libboost_iostreams*$SHLIBSUFFIX*
	lib/libboost_system*$SHLIBSUFFIX*
	lib/libboost_chrono*$SHLIBSUFFIX*

	lib/libIECore*$SHLIBSUFFIX
	
	lib/libIex*$SHLIBSUFFIX*
	lib/libHalf*$SHLIBSUFFIX*
	lib/libImath*$SHLIBSUFFIX*
	lib/libIlmImf*$SHLIBSUFFIX*
	lib/libIlmThread*$SHLIBSUFFIX*

	lib/libtiff*$SHLIBSUFFIX*
	lib/libfreetype*$SHLIBSUFFIX*
	lib/libjpeg*$SHLIBSUFFIX*
	lib/libpng*$SHLIBSUFFIX*

	lib/libOpenImageIO*$SHLIBSUFFIX*
	lib/libOpenColorIO*$SHLIBSUFFIX*

	lib/libLLVM*$SHLIBSUFFIX*
	lib/libosl*

	lib/libpython*$SHLIBSUFFIX*
	lib/Python.framework
	lib/python$PYTHON_VERSION

	lib/libGLEW*$SHLIBSUFFIX*
	lib/libtbb*$SHLIBSUFFIX*

	lib/libhdf5*$SHLIBSUFFIX*

	lib/libpyside*$SHLIBSUFFIX*
	lib/libshiboken*$SHLIBSUFFIX*

	lib/libQtCore*
	lib/libQtGui*
	lib/libQtOpenGL*
	lib/QtCore.framework
	lib/QtGui.framework
	lib/QtOpenGL.framework

	lib/libxerces-c*$SHLIBSUFFIX*

	fonts
	ops
	procedurals
	resources
	shaders

	openColorIO

	glsl/IECoreGL
	glsl/*.frag
	glsl/*.vert

	doc/licenses
	doc/cortex/html
	doc/osl*

	python/IECore*
	python/shiboken.so
	python/PySide/*.py
	python/PySide/QtCore.so
	python/PySide/QtGui.so
	python/PySide/QtOpenGL.so
	python/PyQt*
	python/OpenGL
	python/PyOpenColorIO*

	include/IECore*
	include/boost
	include/GL
	include/OpenEXR
	include/python*
	include/tbb
	include/OSL
	include/OpenImageIO
	include/OpenColorIO
	include/QtCore
	include/QtGui
	include/QtOpenGL

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

archiveName=`basename $BUILD_DIR`-$PLATFORM.tar.gz

tar -c -z -f ../$archiveName $manifest

