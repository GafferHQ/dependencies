cd %~dp0%..\glew-2.0.0

mkdir %BUILD_DIR%\doc\licenses
copy LICENSE.txt %BUILD_DIR%\doc\licenses\glew

mkdir gafferBuild
cd gafferBuild

cmake -Wno-dev -G %CMAKE_GENERATOR% -DCMAKE_INSTALL_PREFIX=%BUILD_DIR% ..\build\cmake
cmake --build . --config %BUILD_TYPE% --target install

cd %ROOT_DIR%
