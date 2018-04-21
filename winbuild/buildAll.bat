@echo off

for %%n in (BUILD_DIR VERSION ARNOLD_ROOT RMAN_ROOT) do (
	if not defined %%n (
		echo ERROR : %%n environment variable not set
		goto error
	)
)

set CMAKE_GENERATOR="Visual Studio 15 2017 Win64"
set BUILD_TYPE=RELEASE
set BOOST_MSVC_VERSION=msvc-14.0

set ROOT_DIR=%~dp0%\..

set ARCHIVE_DIR=%ROOT_DIR%\archives

set ALEMBIC_VERSION=1.7.5
set BLOSC_VERSION=1.7.0
set APPLESEED_VERSION=1.8.1-beta
set HDF5_VERSION=1.8.20
set JPEG_VERSION=1.5.2
set OSL_VERSION=1.8.12
set TBB_VERSION=2017_U6
set USD_VERSION=0.8.2

cd ROOT_DIR

echo ===============================================================================
echo Building ZLIB...
echo ===============================================================================
call %ROOT_DIR%\winbuild\buildZLIB.bat
if %ERRORLEVEL% NEQ 0 (
	echo "Error(s) building ZLIB"
	exit /b %ERRORLEVEL%
)
echo ===============================================================================
echo Building OpenSSL...
echo ===============================================================================
call %ROOT_DIR%\winbuild\buildOpenSSL.bat
if %ERRORLEVEL% NEQ 0 (
	echo "Error(s) building OpenSSL"
	exit /b %ERRORLEVEL%
)
echo ===============================================================================
echo Building Python...
echo ===============================================================================
call %ROOT_DIR%\winbuild\buildPython.bat
if %ERRORLEVEL% NEQ 0 (
	echo "Error(s) building Python"
	exit /b %ERRORLEVEL%
)
echo ===============================================================================
echo Building Subprocess32...
echo ===============================================================================
call %ROOT_DIR%\winbuild\buildSubprocess32.bat
if %ERRORLEVEL% NEQ 0 (
	echo "Error(s) building Subprocess32"
	exit /b %ERRORLEVEL%
)
echo ===============================================================================
echo Building Boost...
echo ===============================================================================
call %ROOT_DIR%\winbuild\buildBoost.bat
if %ERRORLEVEL% NEQ 0 (
	echo "Error(s) building Boost"
	exit /b %ERRORLEVEL%
)
echo ===============================================================================
echo Building JPEG...
echo ===============================================================================
call %ROOT_DIR%\winbuild\buildJPEG.bat
if %ERRORLEVEL% NEQ 0 (
	echo "Error(s) building libJPEG"
	exit /b %ERRORLEVEL%
)
echo ===============================================================================
echo Building TIFF...
echo ===============================================================================
call %ROOT_DIR%\winbuild\buildTIFF.bat
if %ERRORLEVEL% NEQ 0 (
	echo "Error(s) building libTIFF"
	exit /b %ERRORLEVEL%
)
echo ===============================================================================
echo Building PNG...
echo ===============================================================================
call %ROOT_DIR%\winbuild\buildPNG.bat
if %ERRORLEVEL% NEQ 0 (
	echo "Error(s) building libPNG"
	exit /b %ERRORLEVEL%
)
echo ===============================================================================
echo Building FreeType...
echo ===============================================================================
call %ROOT_DIR%\winbuild\buildFreeType.bat
if %ERRORLEVEL% NEQ 0 (
	echo "Error(s) building libFreetype"
	exit /b %ERRORLEVEL%
)
echo ===============================================================================
echo Building TBB...
echo ===============================================================================
call %ROOT_DIR%\winbuild\buildTBB.bat
if %ERRORLEVEL% NEQ 0 (
	echo "Error(s) building TBB"
	exit /b %ERRORLEVEL%
)
echo ===============================================================================
echo Building EXR...
echo ===============================================================================
call %ROOT_DIR%\winbuild\buildEXR.bat
if %ERRORLEVEL% NEQ 0 (
	echo "Error(s) building OpenEXR"
	exit /b %ERRORLEVEL%
)
echo ===============================================================================
echo Building Fonts...
echo ===============================================================================
call %ROOT_DIR%\winbuild\buildFonts.bat
if %ERRORLEVEL% NEQ 0 (
	echo "Error(s) building Fonts"
	exit /b %ERRORLEVEL%
)
echo ===============================================================================
echo Building GLEW...
echo ===============================================================================
call %ROOT_DIR%\winbuild\buildGLEW.bat
if %ERRORLEVEL% NEQ 0 (
	echo "Error(s) building GLEW"
	exit /b %ERRORLEVEL%
)
echo ===============================================================================
echo Building OCIO...
echo ===============================================================================
call %ROOT_DIR%\winbuild\buildOCIO.bat
if %ERRORLEVEL% NEQ 0 (
	echo "Error(s) building OCIO"
	exit /b %ERRORLEVEL%
)
echo ===============================================================================
echo Building OIIO...
echo ===============================================================================
call %ROOT_DIR%\winbuild\buildOIIO.bat
if %ERRORLEVEL% NEQ 0 (
	echo "Error(s) building OIIO"
	exit /b %ERRORLEVEL%
)
echo ===============================================================================
echo Building Blosc...
echo ===============================================================================
call %ROOT_DIR%\winbuild\buildBlosc.bat
if %ERRORLEVEL% NEQ 0 (
	echo "Error(s) building Blosc"
	exit /b %ERRORLEVEL%
)
echo ===============================================================================
echo Building OpenVDB...
echo ===============================================================================
call %ROOT_DIR%\winbuild\buildVDB.bat
if %ERRORLEVEL% NEQ 0 (
	echo "Error(s) building OpenVDB"
	exit /b %ERRORLEVEL%
)
echo ===============================================================================
echo Building LLVM...
echo ===============================================================================
call %ROOT_DIR%\winbuild\buildLLVM.bat
if %ERRORLEVEL% NEQ 0 (
	echo "Error(s) building LLVM"
	exit /b %ERRORLEVEL%
)
echo ===============================================================================
echo Building OSL...
echo ===============================================================================
call %ROOT_DIR%\winbuild\buildOSL.bat
if %ERRORLEVEL% NEQ 0 (
	echo "Error(s) building OSL"
	exit /b %ERRORLEVEL%
)
echo ===============================================================================
echo Building HDF5...
echo ===============================================================================
call %ROOT_DIR%\winbuild\buildHDF5.bat
if %ERRORLEVEL% NEQ 0 (
	echo "Error(s) building HDF5"
	exit /b %ERRORLEVEL%
)
echo ===============================================================================
echo Building Alembic...
echo ===============================================================================
call %ROOT_DIR%\winbuild\buildAlembic.bat
if %ERRORLEVEL% NEQ 0 (
	echo "Error(s) building Alembic"
	exit /b %ERRORLEVEL%
)
echo ===============================================================================
echo Building Xerces...
echo ===============================================================================
call %ROOT_DIR%\winbuild\buildXerces.bat
if %ERRORLEVEL% NEQ 0 (
	echo "Error(s) building Xerces"
	exit /b %ERRORLEVEL%
)
echo ===============================================================================
echo Building Appleseed...
echo ===============================================================================
call %ROOT_DIR%\winbuild\buildAppleseed.bat
if %ERRORLEVEL% NEQ 0 (
	echo "Error(s) building Appleseed"
	exit /b %ERRORLEVEL%
)
echo ===============================================================================
echo Building PyOpenGL...
echo ===============================================================================
call %ROOT_DIR%\winbuild\buildPyOpenGL.bat
if %ERRORLEVEL% NEQ 0 (
	echo "Error(s) building PyOpenGL"
	exit /b %ERRORLEVEL%
)
echo ===============================================================================
echo Building Qt...
echo ===============================================================================
call %ROOT_DIR%\winbuild\buildQt.bat
if %ERRORLEVEL% NEQ 0 (
	echo "Error(s) building Qt"
	exit /b %ERRORLEVEL%
)
echo ===============================================================================
echo Building PySide...
echo ===============================================================================
call %ROOT_DIR%\winbuild\buildPySide.bat
if %ERRORLEVEL% NEQ 0 (
	echo "Error(s) building PySide"
	exit /b %ERRORLEVEL%
)
echo ===============================================================================
echo Building Qt.py...
echo ===============================================================================
call %ROOT_DIR%\winbuild\buildQtPy.bat
if %ERRORLEVEL% NEQ 0 (
	echo "Error(s) building QtPy"
	exit /b %ERRORLEVEL%
)
echo ===============================================================================
echo Building Package...
echo ===============================================================================
rem call %ROOT_DIR%\winbuild\buildPackage.bat
rem if %ERRORLEVEL% NEQ 0 (
rem 	echo "Error(s) building Dependencies Package"
rem 	exit /b %ERRORLEVEL%
rem )
