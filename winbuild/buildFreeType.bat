cd %~dp0%..\freetype-2.7.1

mkdir %BUILD_DIR%\doc\licenses
copy docs\FTL.TXT %BUILD_DIR%\doc\licenses\freetype

mkdir gafferBuild
cd gafferBuild

cmake -Wno-dev -G %CMAKE_GENERATOR% -DCMAKE_INSTALL_PREFIX=%BUILD_DIR% ..
cmake --build . --config %BUILD_TYPE% --target install

cd %ROOT_DIR%
