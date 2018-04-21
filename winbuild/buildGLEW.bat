SETLOCAL

cd %ROOT_DIR%\glew-2.1.0

mkdir %BUILD_DIR%\doc\licenses
copy LICENSE.txt %BUILD_DIR%\doc\licenses\glew

mkdir gafferBuild
cd gafferBuild
del /f CMakeCache.txt

cmake -Wno-dev -G %CMAKE_GENERATOR% -DCMAKE_INSTALL_PREFIX=%BUILD_DIR% ..\build\cmake
if %ERRORLEVEL% NEQ 0 (exit /b %ERRORLEVEL%)
cmake --build . --config %BUILD_TYPE% --target install
if %ERRORLEVEL% NEQ 0 (exit /b %ERRORLEVEL%)

ENDLOCAL