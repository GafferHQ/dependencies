cd %~dp0%..\libpng-1.6.3

mkdir %BUILD_DIR%\doc\licenses
copy LICENSE %BUILD_DIR%\doc\licenses\libpng

mkdir gafferBuild
cd gafferBuild

cmake -Wno-dev -G %CMAKE_GENERATOR% -DZLIB_INCLUDE_DIR=%BUILD_DIR%\\include -DZLIB_LIBRARY=%BUILD_DIR%\\lib\\zlibstatic.lib -DCMAKE_INSTALL_PREFIX=%BUILD_DIR% ..
cmake --build . --config %BUILD_TYPE% --target install

cd %ROOT_DIR%
