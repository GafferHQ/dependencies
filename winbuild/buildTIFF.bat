SETLOCAL

cd %ROOT_DIR%\tiff-4.0.8

mkdir %BUILD_DIR%\doc\licenses
copy COPYRIGHT %BUILD_DIR%\doc\licenses\libtiff

nmake /f makefile.vc
if %ERRORLEVEL% NEQ 0 (exit /b %ERRORLEVEL%)

copy libtiff\libtiff.lib %BUILD_DIR%\lib
copy libtiff\*.h %BUILD_DIR%\include

rem mkdir gafferBuild
rem cd gafferBuild
rem rm -f CMakeCache.txt

rem cmake -Wno-dev -G %CMAKE_GENERATOR% -DCMAKE_BUILD_TYPE=%BUILD_TYPE%  -DCMAKE_PREFIX_PATH=%BUILD_DIR% -DCMAKE_INSTALL_PREFIX=%BUILD_DIR% ..
rem cmake --build . --config %BUILD_TYPE% --target install

ENDLOCAL
