cd %~dp0%..\tbb-4.4.6

mkdir %BUILD_DIR%\doc\licenses
copy COPYING %BUILD_DIR%\doc\licenses\tbb

mkdir gafferBuild
cd gafferBuild
rm -f CMakeCache.txt

cmake -Wno-dev -G %CMAKE_GENERATOR% -DCMAKE_BUILD_TYPE=%BUILD_TYPE%  -DCMAKE_PREFIX_PATH=%BUILD_DIR% -DCMAKE_INSTALL_PREFIX=%BUILD_DIR% -DTBB_BUILD_TESTS=OFF ..
cmake --build . --config %BUILD_TYPE% --target install


cd %ROOT_DIR%
