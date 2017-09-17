
cd %~dp0%..\ilmbase-2.2.0

mkdir %BUILD_DIR%\doc\licenses
copy COPYING %BUILD_DIR%\doc\licenses\ilmbase

mkdir gafferBuild
cd gafferBuild

cmake -Wno-dev -G %CMAKE_GENERATOR% -DBUILD_SHARED_LIBS=ON -DCMAKE_INSTALL_PREFIX=%BUILD_DIR% ..
cmake --build . --config %BUILD_TYPE% --target install

cd %ROOT_DIR%\..\openexr-2.2.0

copy LICENSE %BUILD_DIR%\doc\licenses\openexr

mkdir gafferBuild
cd gafferBuild

rem We need to have the lib dir in the system path so that the DLL's that OpenEXR relies on in the build process can be found
set BACKUP_PATH=%PATH%
set PATH=%PATH%;%BUILD_DIR%\lib

cmake -Wno-dev -G %CMAKE_GENERATOR% -DBUILD_SHARED_LIBS=ON -DILMBASE_PACKAGE_PREFIX=%BUILD_DIR% -DZLIB_INCLUDE_DIR=%BUILD_DIR%\include -DZLIB_LIBRARY=%BUILD_DIR%\lib\zlib.lib -DCMAKE_INSTALL_PREFIX=%BUILD_DIR% ..
cmake --build . --config %BUILD_TYPE% --target install

rem Restore path
set PATH=%BACKUP_PATH%

cd %ROOT_DIR%
