SETLOCAL

set USD_VERSION=0.8.2

set ARCHIVE_ROOT_NAME=USD-%USD_VERSION%
set WORKING_DIR=%ROOT_DIR%\%ARCHIVE_ROOT_NAME%

mkdir %WORKING_DIR%
copy %ARCHIVE_DIR%\%ARCHIVE_ROOT_NAME%.tar.gz %ROOT_DIR%

cd %ROOT_DIR%

%ROOT_DIR%\winbuild\7zip\7za.exe e -aoa %ARCHIVE_ROOT_NAME%.tar.gz
if %ERRORLEVEL% NEQ 0 (exit /b %ERRORLEVEL%)
%ROOT_DIR%\winbuild\7zip\7za.exe x -aoa %ARCHIVE_ROOT_NAME%.tar
if %ERRORLEVEL% NEQ 0 (exit /b %ERRORLEVEL%)

cd %ROOT_DIR%
%ROOT_DIR%\winbuild\patch\bin\patch -f -p1 < %ROOT_DIR%\winbuild\usd_patch_1.diff

cd %WORKING_DIR%

mkdir %BUILD_DIR%\doc\licenses
copy LICENSE.txt %BUILD_DIR%\doc\licenses\usd

mkdir gafferBuild
cd gafferBuild
del /f CMakeCache.txt

cmake -Wno-dev -G %CMAKE_GENERATOR% -DCMAKE_BUILD_TYPE=%BUILD_TYPE% -DCMAKE_PREFIX_PATH=%BUILD_DIR% -DCMAKE_INSTALL_PREFIX=%BUILD_DIR% -DBoost_NO_BOOST_CMAKE=TRUE -DPXR_BUILD_IMAGING=FALSE -DPXR_BUILD_TESTS=FALSE ..
if %ERRORLEVEL% NEQ 0 (exit /b %ERRORLEVEL%)
cmake --build . --config %BUILD_TYPE% --target install
if %ERRORLEVEL% NEQ 0 (exit /b %ERRORLEVEL%)

ENDLOCAL