@echo off

for %%n in (BUILD_DIR ARNOLD_ROOT RMAN_ROOT) do (
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

cd ROOT_DIR

echo ===============================================================================
echo Building ZLIB...
echo ===============================================================================
cd %ROOT_DIR% && python build/build.py --project Zlib --buildDir %BUILD_DIR%
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
cd %ROOT_DIR% && python build/build.py --project Python --buildDir %BUILD_DIR%
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
cd %ROOT_DIR% && python build/build.py --project Boost --buildDir %BUILD_DIR%
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
cd %ROOT_DIR% && python build/build.py --project FreeType --buildDir %BUILD_DIR%
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
cd %ROOT_DIR% && python build/build.py --project OpenColorIO --buildDir %BUILD_DIR%
if %ERRORLEVEL% NEQ 0 (
	echo "Error(s) building OCIO"
	exit /b %ERRORLEVEL%
)
echo ===============================================================================
echo Building OIIO...
echo ===============================================================================
cd %ROOT_DIR% && python build/build.py --project OpenImageIO --buildDir %BUILD_DIR%
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
cd %ROOT_DIR% && python build/build.py --project LLVM --buildDir %BUILD_DIR%
if %ERRORLEVEL% NEQ 0 (
	echo "Error(s) building LLVM"
	exit /b %ERRORLEVEL%
)
echo ===============================================================================
echo Building OSL...
echo ===============================================================================
cd %ROOT_DIR% && python build/build.py --project OpenShadingLanguage --buildDir %BUILD_DIR%
if %ERRORLEVEL% NEQ 0 (
	echo "Error(s) building OSL"
	exit /b %ERRORLEVEL%
)
echo ===============================================================================
echo Building HDF5...
echo ===============================================================================
cd %ROOT_DIR% && python build/build.py --project HDF5 --buildDir %BUILD_DIR%
if %ERRORLEVEL% NEQ 0 (
	echo "Error(s) building HDF5"
	exit /b %ERRORLEVEL%
)
echo ===============================================================================
echo Building Alembic...
echo ===============================================================================
cd %ROOT_DIR% && python build/build.py --project Alembic --buildDir %BUILD_DIR%
if %ERRORLEVEL% NEQ 0 (
	echo "Error(s) building Alembic"
	exit /b %ERRORLEVEL%
)
echo ===============================================================================
echo Building Xerces...
echo ===============================================================================
cd %ROOT_DIR% && python build/build.py --project Xerces --buildDir %BUILD_DIR%
if %ERRORLEVEL% NEQ 0 (
	echo "Error(s) building Xerces"
	exit /b %ERRORLEVEL%
)
echo ===============================================================================
echo Building Appleseed...
echo ===============================================================================
cd %ROOT_DIR% && python build/build.py --project Appleseed --buildDir %BUILD_DIR%
if %ERRORLEVEL% NEQ 0 (
	echo "Error(s) building Appleseed"
	exit /b %ERRORLEVEL%
)
echo ===============================================================================
echo Building Resources...
echo ===============================================================================
cd %ROOT_DIR% && python build/build.py --project GafferResources --buildDir %BUILD_DIR%
if %ERRORLEVEL% NEQ 0 (
	echo "Error(s) building Resources"
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
echo Building CMark...
echo ===============================================================================
call %ROOT_DIR%\winbuild\buildCMark.bat
if %ERRORLEVEL% NEQ 0 (
	echo "Error(s) building CMark"
	exit /b %ERRORLEVEL%
)
echo ===============================================================================
echo Building Package...
echo ===============================================================================
call %ROOT_DIR%\winbuild\buildPackage.bat
if %ERRORLEVEL% NEQ 0 (
	echo "Error(s) building Dependencies Package"
	exit /b %ERRORLEVEL%
)
