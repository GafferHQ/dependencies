mkdir %BUILD_DIR%\resources\cortex
copy %ROOT_DIR%\cortex-10.0.0-a8\tileset_2048.dat %BUILD_DIR%\resources\cortex

cd %~dp0%..\..\cortex

mkdir %BUILD_DIR%\doc\licenses
copy LICENSE %BUILD_DIR%\doc\licenses\cortex

copy contrib\cmake\CMakeLists.txt CMakeLists.txt

mkdir gafferBuild
cd gafferBuild
del /f CMakeCache.txt

cmake -Wno-dev -G %CMAKE_GENERATOR% -DCMAKE_INSTALL_PREFIX=%BUILD_DIR% -DPYTHON_LIBRARY=%BUILD_DIR%\lib -DPYTHON_INCLUDE_DIR=%BUILD_DIR%\include -DILMBASE_LOCATION=%BUILD_DIR% -DOPENEXR_LOCATION=%BUILD_DIR% -DWITH_IECORE_GL=1 -DWITH_IECORE_IMAGE=1 -DWITH_IECORE_SCENE=1 -DWITH_IECORE_ARNOLD=0 -DARNOLD_ROOT=%ARNOLD_ROOT% -DWITH_IECORE_ALEMBIC=1 -DALEMBIC_ROOT=%BUILD_DIR% -DWITH_IECORE_APPLESEED=1 -DAPPLESEED_INCLUDE_DIR=%BUILD_DIR%\appleseed\include -DAPPLESEED_LIBRARY=%BUILD_DIR%\appleseed\lib\appleseed.lib ..
if %ERRORLEVEL% NEQ 0 (exit /b %ERRORLEVEL%)
cmake --build . --config %BUILD_TYPE% --target install
if %ERRORLEVEL% NEQ 0 (exit /b %ERRORLEVEL%)
