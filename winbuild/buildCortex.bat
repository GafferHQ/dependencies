mkdir %BUILD_DIR%\resources\cortex
cp %~dp0%..\cortex-9.21.1\tileset_2048.dat %BUILD_DIR%\resources\cortex

cd %~dp0%..\..\cortex

mkdir %BUILD_DIR%\doc\licenses
copy LICENSE %BUILD_DIR%\doc\licenses\cortex

mkdir gafferBuild
cd gafferBuild

cmake -Wno-dev -G %CMAKE_GENERATOR% -DCMAKE_INSTALL_PREFIX=%BUILD_DIR% -DPYTHON_LIBRARY=%BUILD_DIR%\lib -DPYTHON_INCLUDE_DIR=%BUILD_DIR%\include -DILMBASE_LOCATION=%BUILD_DIR% -DOPENEXR_LOCATION=%BUILD_DIR% -DWITH_GL=1 -DWITH_ARNOLD=1 -DARNOLD_ROOT=%ARNOLD_ROOT% -DWITH_ALEMBIC=1 -DALEMBIC_ROOT=%BUILD_DIR% -DWITH_APPLESEED=1 -DAPPLESEED_ROOT=%BUILD_DIR%\appleseed -DWITH_RMAN=0 RMAN_ROOT=%RMAN_ROOT% ..
cmake --build . --config %BUILD_TYPE% --target install

cd %ROOT_DIR%
