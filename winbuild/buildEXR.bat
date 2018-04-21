SETLOCAL 

echo ===============================================================================
echo Building ILMBase...
echo ===============================================================================


cd %ROOT_DIR%\ilmbase-2.2.0

mkdir %BUILD_DIR%\doc\licenses
copy COPYING %BUILD_DIR%\doc\licenses\ilmbase

mkdir gafferBuild
cd gafferBuild
del /f CMakeCache.txt

cmake -Wno-dev -G %CMAKE_GENERATOR% -DBUILD_SHARED_LIBS=ON -DCMAKE_INSTALL_PREFIX=%BUILD_DIR% ..
if %ERRORLEVEL% NEQ 0 (exit /b %ERRORLEVEL%)
cmake --build . --config %BUILD_TYPE% --target install
if %ERRORLEVEL% NEQ 0 (exit /b %ERRORLEVEL%)


echo ===============================================================================
echo Building OpenEXR...
echo ===============================================================================

cd %ROOT_DIR%\openexr-2.2.0

copy LICENSE %BUILD_DIR%\doc\licenses\openexr

mkdir gafferBuild
cd gafferBuild
del /f CMakeCache.txt

SETLOCAL

rem We need to have the lib dir in the system path so that the DLL's that OpenEXR relies on in the build process can be found
set PATH=%PATH%;%BUILD_DIR%\lib

cmake -Wno-dev -G %CMAKE_GENERATOR% -DBUILD_SHARED_LIBS=ON -DILMBASE_PACKAGE_PREFIX=%BUILD_DIR% -DZLIB_INCLUDE_DIR=%BUILD_DIR%\include -DZLIB_LIBRARY=%BUILD_DIR%\lib\zlib.lib -DCMAKE_INSTALL_PREFIX=%BUILD_DIR% ..
if %ERRORLEVEL% NEQ 0 (exit /b %ERRORLEVEL%)
cmake --build . --config %BUILD_TYPE% --target install
if %ERRORLEVEL% NEQ 0 (exit /b %ERRORLEVEL%)

ENDLOCAL

echo ===============================================================================
echo Building PyILMBase...
echo ===============================================================================

cd %ROOT_DIR%\pyilmbase-2.2.0

copy COPYING %BUILD_DIR%\doc\licenses\pyilmbase

mkdir gafferBuild
cd gafferBuild
del /f CMakeCache.txt

cmake -Wno-dev -G %CMAKE_GENERATOR% -DCMAKE_BUILD_TYPE=%BUILD_TYPE% -DCMAKE_INSTALL_PREFIX=%BUILD_DIR% -DCMAKE_PREFIX_PATH=%BUILD_DIR% -DPYTHON_LIBRARY=%BUILD_DIR%\lib -DPYTHON_INCLUDE_DIR=%BUILD_DIR%\include -DBOOST_ROOT=%BUILD_DIR% -DILMBASE_PACKAGE_PREFIX=%BUILD_DIR% ..
if %ERRORLEVEL% NEQ 0 (exit /b %ERRORLEVEL%)
cmake --build . --config %BUILD_TYPE% --target install
if %ERRORLEVEL% NEQ 0 (exit /b %ERRORLEVEL%)

mkdir %BUILD_DIR%\python
move %BUILD_DIR%\lib\python2.7\site-packages\iex.pyd %BUILD_DIR%\python
if %ERRORLEVEL% NEQ 0 (exit /b %ERRORLEVEL%)
move %BUILD_DIR%\lib\python2.7\site-packages\imath.pyd %BUILD_DIR%\python
if %ERRORLEVEL% NEQ 0 (exit /b %ERRORLEVEL%)

ENDLOCAL