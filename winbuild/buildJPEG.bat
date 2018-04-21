cd %ROOT_DIR%\libjpeg-turbo-1.5.2

mkdir %BUILD_DIR%\doc\licenses
copy LICENSE.md %BUILD_DIR%\doc\licenses\libjpeg

mkdir gafferBuild
cd gafferBuild
del /f CMakeCache.txt

cmake -Wno-dev -G %CMAKE_GENERATOR% -DCMAKE_BUILD_TYPE=%BUILD_TYPE% -DWITH_SIMD=OFF -DCMAKE_INSTALL_PREFIX=%BUILD_DIR% ..
if %ERRORLEVEL% NEQ 0 (exit /b %ERRORLEVEL%)
cmake --build . --config %BUILD_TYPE% --target install
if %ERRORLEVEL% NEQ 0 (exit /b %ERRORLEVEL%)
