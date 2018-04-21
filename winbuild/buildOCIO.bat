cd %ROOT_DIR%\OpenColorIO-1.0.9

mkdir %BUILD_DIR%\doc\licenses
copy LICENSE %BUILD_DIR%\doc\licenses\openColorIO

mkdir gafferBuild
cd gafferBuild
del /f CMakeCache.txt

set "BUILD_DIR=%BUILD_DIR:\=/%"
set PATH=%PATH%;%ROOT_DIR%\winbuild\patch\bin

cmake -Wno-dev -G %CMAKE_GENERATOR% -DCMAKE_BUILD_TYPE=%BUILD_TYPE% -DCMAKE_INSTALL_PREFIX=%BUILD_DIR% -DPYTHON=%BUILD_DIR%/bin/python.exe -DOCIO_USE_BOOST_PTR=1 -DOCIO_BUILD_TRUELIGHT=OFF -DOCIO_BUILD_APPS=OFF -DOCIO_BUILD_NUKE=OFF -DOCIO_BUILD_STATIC=OFF -DOCIO_BUILD_SHARED=ON -DOCIO_BUILD_PYGLUE=ON -DPYTHON_VERSION=2.7 -DPYTHON_INCLUDE=%BUILD_DIR%/include -DPYTHON_LIB=%BUILD_DIR%/lib -DOCIO_PYGLUE_LINK=ON ..
if %ERRORLEVEL% NEQ 0 (exit /b %ERRORLEVEL%)
cmake --build . --config %BUILD_TYPE% --target install
if %ERRORLEVEL% NEQ 0 (exit /b %ERRORLEVEL%)

set "BUILD_DIR=%BUILD_DIR:/=\%"

mkdir %BUILD_DIR%\python
move %BUILD_DIR%\PyOpenColorIO.dll %BUILD_DIR%\python\PyOpenColorIO.pyd

mkdir %BUILD_DIR%\openColorIO
mkdir %BUILD_DIR%\openColorIO\luts
copy %ROOT_DIR%\imageworks-OpenColorIO-Configs-f931d77\nuke-default\config.ocio %BUILD_DIR%\\openColorIO\\
copy %ROOT_DIR%\imageworks-OpenColorIO-Configs-f931d77\nuke-default\luts %BUILD_DIR%\\openColorIO\\luts
