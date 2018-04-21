SETLOCAL

cd %ROOT_DIR%\llvm-3.4

mkdir build-release
cd build-release
del /f CMakeCache.txt

cmake -Wno-dev -G %CMAKE_GENERATOR% -DCMAKE_BUILD_TYPE=%BUILD_TYPE% -DCMAKE_INSTALL_PREFIX=%BUILD_DIR% -DBUILD_SHARED_LIBS=OFF -DLLVM_REQUIRES_RTTI=ON -DLLVM_TARGETS_TO_BUILD="X86" ..
if %ERRORLEVEL% NEQ 0 (exit /b %ERRORLEVEL%)
cmake --build . --config %BUILD_TYPE% --target install
if %ERRORLEVEL% NEQ 0 (exit /b %ERRORLEVEL%)

ENDLOCAL