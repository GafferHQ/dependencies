SETLOCAL

cd %ROOT_DIR%\Python-2.7.13

mkdir %BUILD_DIR%\doc\licenses
copy LICENSE %BUILD_DIR%\doc\licenses\python

mkdir gafferBuild
cd gafferBuild
del /f CMakeCache.txt

cmake -Wno-dev -G %CMAKE_GENERATOR% -DCMAKE_BUILD_TYPE=%BUILD_TYPE% -DCMAKE_INSTALL_PREFIX=%BUILD_DIR% -DPYTHON_VERSION=2.7.13 -DDOWNLOAD_SOURCES=OFF -DBUILD_LIBPYTHON_SHARED=ON -DPy_UNICODE_SIZE=4 -DUSE_LIB64=ON -DINSTALL_TEST=OFF ..\python-cmake-buildsystem
if %ERRORLEVEL% NEQ 0 (exit /b %ERRORLEVEL%)
cmake --build . --config %BUILD_TYPE% --target install
if %ERRORLEVEL% NEQ 0 (exit /b %ERRORLEVEL%)

rem Move stuff around
copy %BUILD_DIR%\libs\*.lib %BUILD_DIR%\lib

ENDLOCAL