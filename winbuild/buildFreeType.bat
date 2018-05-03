SETLOCAL

cd %ROOT_DIR%\freetype-2.7.1

mkdir %BUILD_DIR%\doc\licenses
copy docs\FTL.TXT %BUILD_DIR%\doc\licenses\freetype

mkdir gafferBuild
cd gafferBuild
del /f CMakeCache.txt

cmake -Wno-dev -G %CMAKE_GENERATOR% -DCMAKE_INSTALL_PREFIX=%BUILD_DIR% ..
if %ERRORLEVEL% NEQ 0 (exit /b %ERRORLEVEL%)
cmake --build . --config %BUILD_TYPE% --target install
if %ERRORLEVEL% NEQ 0 (exit /b %ERRORLEVEL%)

ENDLOCAL