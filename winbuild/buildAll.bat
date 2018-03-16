@echo off

for %%n in (BUILD_DIR VERSION ARNOLD_ROOT RMAN_ROOT) do (
	if not defined %%n (
		echo ERROR : %%n environment variable not set
		goto error
	)
)

set ROOT_DIR=%~dp0%
set CMAKE_GENERATOR="Visual Studio 15 2017 Win64"
set BUILD_TYPE=RELEASE
set BOOST_MSVC_VERSION=msvc-14.0
set EXACT_MSVC_VERSION=14.12.25827
cd ROOT_DIR
echo ===============================================================================
echo Building ZLIB...
echo ===============================================================================
call buildZLIB.bat
if %ERRORLEVEL% NEQ 0 (
	echo "Error(s) building ZLIB"
	exit /b %ERRORLEVEL%
)
echo ===============================================================================
echo Building OpenSSL...
echo ===============================================================================
call buildOpenSSL.bat
if %ERRORLEVEL% NEQ 0 (
	echo "Error(s) building OpenSSL"
	exit /b %ERRORLEVEL%
)
echo ===============================================================================
echo Building Python...
echo ===============================================================================
call buildPython.bat
if %ERRORLEVEL% NEQ 0 (
	echo "Error(s) building Python"
	exit /b %ERRORLEVEL%
)
echo ===============================================================================
echo Building Subprocess32...
echo ===============================================================================
call buildSubprocess32.bat
if %ERRORLEVEL% NEQ 0 (
	echo "Error(s) building Subprocess32"
	exit /b %ERRORLEVEL%
)
echo ===============================================================================
echo Building Boost...
echo ===============================================================================
call buildBoost.bat
if %ERRORLEVEL% NEQ 0 (
	echo "Error(s) building Boost"
	exit /b %ERRORLEVEL%
)
echo ===============================================================================
echo Building JPEG...
echo ===============================================================================
call buildJPEG.bat
if %ERRORLEVEL% NEQ 0 (
	echo "Error(s) building libJPEG"
	exit /b %ERRORLEVEL%
)
echo ===============================================================================
echo Building TIFF...
echo ===============================================================================
call buildTIFF.bat
if %ERRORLEVEL% NEQ 0 (
	echo "Error(s) building libTIFF"
	exit /b %ERRORLEVEL%
)
echo ===============================================================================
echo Building PNG...
echo ===============================================================================
call buildPNG.bat
if %ERRORLEVEL% NEQ 0 (
	echo "Error(s) building libPNG"
	exit /b %ERRORLEVEL%
)
echo ===============================================================================
echo Building FreeType...
echo ===============================================================================
call buildFreeType.bat
if %ERRORLEVEL% NEQ 0 (
	echo "Error(s) building libFreetype"
	exit /b %ERRORLEVEL%
)
echo ===============================================================================
echo Building TBB...
echo ===============================================================================
call buildTBB.bat
if %ERRORLEVEL% NEQ 0 (
	echo "Error(s) building TBB"
	exit /b %ERRORLEVEL%
)
echo ===============================================================================
echo Building EXR...
echo ===============================================================================
call buildEXR.bat
if %ERRORLEVEL% NEQ 0 (
	echo "Error(s) building OpenEXR"
	exit /b %ERRORLEVEL%
)
echo ===============================================================================
echo Building Fonts...
echo ===============================================================================
call buildFonts.bat
if %ERRORLEVEL% NEQ 0 (
	echo "Error(s) building Fonts"
	exit /b %ERRORLEVEL%
)
echo ===============================================================================
echo Building GLEW...
echo ===============================================================================
call buildGLEW.bat
if %ERRORLEVEL% NEQ 0 (
	echo "Error(s) building GLEW"
	exit /b %ERRORLEVEL%
)
echo ===============================================================================
echo Building OCIO...
echo ===============================================================================
call buildOCIO.bat
if %ERRORLEVEL% NEQ 0 (
	echo "Error(s) building OCIO"
	exit /b %ERRORLEVEL%
)
echo ===============================================================================
echo Building OIIO...
echo ===============================================================================
call buildOIIO.bat
if %ERRORLEVEL% NEQ 0 (
	echo "Error(s) building OIIO"
	exit /b %ERRORLEVEL%
)
echo ===============================================================================
echo Building Blosc...
echo ===============================================================================
call buildBlosc.bat
if %ERRORLEVEL% NEQ 0 (
	echo "Error(s) building Blosc"
	exit /b %ERRORLEVEL%
)
echo ===============================================================================
echo Building OpenVDB...
echo ===============================================================================
call buildVDB.bat
if %ERRORLEVEL% NEQ 0 (
	echo "Error(s) building OpenVDB"
	exit /b %ERRORLEVEL%
)
echo ===============================================================================
echo Building LLVM...
echo ===============================================================================
call buildLLVM.bat
if %ERRORLEVEL% NEQ 0 (
	echo "Error(s) building LLVM"
	exit /b %ERRORLEVEL%
)
echo ===============================================================================
echo Building OSL...
echo ===============================================================================
call buildOSL.bat
if %ERRORLEVEL% NEQ 0 (
	echo "Error(s) building OSL"
	exit /b %ERRORLEVEL%
)
echo ===============================================================================
echo Building HDF5...
echo ===============================================================================
call buildHDF5.bat
if %ERRORLEVEL% NEQ 0 (
	echo "Error(s) building HDF5"
	exit /b %ERRORLEVEL%
)
echo ===============================================================================
echo Building Alembic...
echo ===============================================================================
call buildAlembic.bat
if %ERRORLEVEL% NEQ 0 (
	echo "Error(s) building Alembic"
	exit /b %ERRORLEVEL%
)
echo ===============================================================================
echo Building Xerces...
echo ===============================================================================
call buildXerces.bat
if %ERRORLEVEL% NEQ 0 (
	echo "Error(s) building Xerces"
	exit /b %ERRORLEVEL%
)
echo ===============================================================================
echo Building Appleseed...
echo ===============================================================================
call buildAppleseed.bat
if %ERRORLEVEL% NEQ 0 (
	echo "Error(s) building Appleseed"
	exit /b %ERRORLEVEL%
)
echo ===============================================================================
echo Building PyOpenGL...
echo ===============================================================================
call buildPyOpenGL.bat
if %ERRORLEVEL% NEQ 0 (
	echo "Error(s) building PyOpenGL"
	exit /b %ERRORLEVEL%
)
echo ===============================================================================
echo Building Qt...
echo ===============================================================================
call buildQt.bat
if %ERRORLEVEL% NEQ 0 (
	echo "Error(s) building Qt"
	exit /b %ERRORLEVEL%
)
echo ===============================================================================
echo Building PySide...
echo ===============================================================================
call buildPySide.bat
if %ERRORLEVEL% NEQ 0 (
	echo "Error(s) building PySide"
	exit /b %ERRORLEVEL%
)
echo ===============================================================================
echo Building Qt.py...
echo ===============================================================================
call buildQtPy.bat
if %ERRORLEVEL% NEQ 0 (
	echo "Error(s) building QtPy"
	exit /b %ERRORLEVEL%
)
echo ===============================================================================
echo Building Package...
echo ===============================================================================
rem call buildPackage.bat
rem if %ERRORLEVEL% NEQ 0 (
rem 	echo "Error(s) building Dependencies Package"
rem 	exit /b %ERRORLEVEL%
rem )
