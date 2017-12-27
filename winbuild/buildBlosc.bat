SETLOCAL
set BLOSC_VERSION=1.7.0
set ARCHIVE_ROOT_NAME=c-blosc-%BLOSC_VERSION%
set ARCHIVE_DIR=%~dp0%..\archives
set WORKING_DIR=%~dp0%..\c-blosc-1.7.0

del %WORKING_DIR%
mkdir %WORKING_DIR%
copy %ARCHIVE_DIR%\%ARCHIVE_ROOT_NAME%.tar.gz  %WORKING_DIR%

cd %WORKING_DIR%

..\winbuild\7zip\7za.exe e %ARCHIVE_ROOT_NAME%.tar.gz
..\winbuild\7zip\7za.exe x %ARCHIVE_ROOT_NAME%.tar

cd %ARCHIVE_ROOT_NAME%

mkdir %BUILD_DIR%\doc\licenses
copy LICENSES\BLOSC.txt %BUILD_DIR%\doc\licenses\blosc

mkdir gafferBuild
cd gafferBuild

cmake -Wno-dev -G %CMAKE_GENERATOR% -DCMAKE_BUILD_TYPE=%BUILD_TYPE% -DCMAKE_INSTALL_PREFIX=%BUILD_DIR% -DBUILD_TESTS=OFF -DBUILD_BENCHMARKS=OFF -DBUILD_STATIC=OFF -DZLIB_ROOT=%BUILD_DIR%\lib ..
cmake --build . --config %BUILD_TYPE% --target install

ENDLOCAL

cd %ROOT_DIR%
