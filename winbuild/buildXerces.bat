SETLOCAL

cd %ROOT_DIR%\xerces-c-3.2.0

mkdir %BUILD_DIR%\doc\licenses
copy LICENSE %BUILD_DIR%\doc\licenses\xerces

rem pushd projects\Win32\VC14.gaffer\xerces-all

rem devenv xerces-all.sln /build "Static Release" /project all

rem popd

rem xcopy /E /Q /Y src\*.hpp %BUILD_DIR%\include\
rem xcopy /E /Q /Y src\*.c %BUILD_DIR%\include\
rem copy "Build\Win64\VC14\Static Release\xerces-c_static_3.lib" %BUILD_DIR%\lib

mkdir gafferBuild
cd gafferBuild
del /f CMakeCache.txt

cmake -Wno-dev -G %CMAKE_GENERATOR% -DCMAKE_INSTALL_PREFIX=%BUILD_DIR% -DBUILD_SHARED_LIBS=OFF ..
if %ERRORLEVEL% NEQ 0 (exit /b %ERRORLEVEL%)
cmake --build . --config %BUILD_TYPE% --target install
if %ERRORLEVEL% NEQ 0 (exit /b %ERRORLEVEL%)

ENDLOCAL