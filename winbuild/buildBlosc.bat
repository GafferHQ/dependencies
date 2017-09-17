cd %~dp0%..\c-blosc-1.11.2

mkdir %BUILD_DIR%\doc\licenses
copy LICENSES\BLOSC.txt %BUILD_DIR%\doc\licenses\blosc

mkdir gafferBuild
cd gafferBuild

cmake -Wno-dev -G %CMAKE_GENERATOR% -DCMAKE_BUILD_TYPE=%BUILD_TYPE% -DCMAKE_INSTALL_PREFIX=%BUILD_DIR% -DBUILD_TESTS=OFF -DBUILD_BENCHMARKS=OFF -DBUILD_STATIC=OFF -DZLIB_ROOT=%BUILD_DIR%\lib ..
cmake --build . --config %BUILD_TYPE% --target install

cd %ROOT_DIR%
