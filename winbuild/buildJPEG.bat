cd %~dp0%..\libjpeg-turbo-1.5.1

mkdir %BUILD_DIR%\doc\licenses
copy LICENSE.md %BUILD_DIR%\doc\licenses\libjpeg

rem mkdir gafferBuild
rem cd gafferBuild
del CMakeCache.txt

cmake -Wno-dev -G %CMAKE_GENERATOR% -DCMAKE_BUILD_TYPE=%BUILD_TYPE% -DWITH_SIMD=OFF -DCMAKE_INSTALL_PREFIX=%BUILD_DIR%
cmake --build . --config %BUILD_TYPE% --target install

cd %ROOT_DIR%
