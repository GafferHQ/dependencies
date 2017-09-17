cd %~dp0%..\hdf5-1.8.11

mkdir %BUILD_DIR%\doc\licenses
copy COPYING %BUILD_DIR%\doc\licenses\hdf5

mkdir gafferBuild
cd gafferBuild

cmake -C ..\config\cmake\cacheinit.cmake -Wno-dev -G %CMAKE_GENERATOR% -DCMAKE_BUILD_TYPE=%BUILD_TYPE% -DCMAKE_INSTALL_PREFIX=%BUILD_DIR% -DHDF5_ENABLE_THREADSAFE=ON -DBUILD_SHARED_LIBS=ON -DBUILD_TESTING=OFF -DHDF5_BUILD_EXAMPLES=OFF -DHDF5_BUILD_FORTRAN=OFF -DHDF5_ENABLE_SZIP_SUPPORT=OFF ..
cmake --build . --config %BUILD_TYPE% --target install

cd %ROOT_DIR%
