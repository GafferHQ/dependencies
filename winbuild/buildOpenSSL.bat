SETLOCAL

cd %ROOT_DIR%\openssl-1.0.2h

mkdir %BUILD_DIR%\doc\licenses
copy LICENSE %BUILD_DIR%\doc\licenses\openssl

mkdir gafferBuild
cd gafferBuild
del /f CMakeCache.txt

cmake -Wno-dev -G %CMAKE_GENERATOR% -DBUILD_OBJECT_LIBRARY_ONLY=ON -DCMAKE_INSTALL_PREFIX=%BUILD_DIR% ..
if %ERRORLEVEL% NEQ 0 (exit /b %ERRORLEVEL%)
cmake --build . --config %BUILD_TYPE% --target all_build
if %ERRORLEVEL% NEQ 0 (exit /b %ERRORLEVEL%)

copy crypto\%BUILD_TYPE%\crypto.lib %BUILD_DIR%\lib
copy ssl\%BUILD_TYPE%\ssl.lib %BUILD_DIR%\lib
xcopy /E /Q /Y include %BUILD_DIR%\include

ENDLOCAL