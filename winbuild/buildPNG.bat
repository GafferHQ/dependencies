SETLOCAL

cd %ROOT_DIR%\libpng-1.6.3

mkdir %BUILD_DIR%\doc\licenses
copy LICENSE %BUILD_DIR%\doc\licenses\libpng

mkdir gafferBuild
cd gafferBuild
del /f CMakeCache.txt

cmake -Wno-dev -G %CMAKE_GENERATOR% -DZLIB_INCLUDE_DIR=%BUILD_DIR%\\include -DZLIB_LIBRARY=%BUILD_DIR%\\lib\\zlibstatic.lib -DCMAKE_INSTALL_PREFIX=%BUILD_DIR% ..
if %ERRORLEVEL% NEQ 0 (exit /b %ERRORLEVEL%)
cmake --build . --config %BUILD_TYPE% --target install
if %ERRORLEVEL% NEQ 0 (exit /b %ERRORLEVEL%)

ENDLOCAL