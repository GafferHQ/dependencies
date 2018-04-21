SETLOCAL

set ARCHIVE_ROOT_NAME=OpenShadingLanguage-Release-%OSL_VERSION%
set WORKING_DIR=%ROOT_DIR%\%ARCHIVE_ROOT_NAME%

mkdir %WORKING_DIR%
copy %ARCHIVE_DIR%\%ARCHIVE_ROOT_NAME%.tar.gz %ROOT_DIR%

cd %ROOT_DIR%

%ROOT_DIR%\winbuild\7zip\7za.exe e -aoa %ARCHIVE_ROOT_NAME%.tar.gz
if %ERRORLEVEL% NEQ 0 (exit /b %ERRORLEVEL%)
%ROOT_DIR%\winbuild\7zip\7za.exe x -aoa %ARCHIVE_ROOT_NAME%.tar
if %ERRORLEVEL% NEQ 0 (exit /b %ERRORLEVEL%)

cd %ROOT_DIR%
rem These two files are created by the following patches
rem Conflicts between Unix and Windows line endings make it
rem easier to just delete them and create them by patch
del %WORKING_DIR%\src\cmake\flexbison.cmake
del %WORKING_DIR%\src\cmake\modules\FindLLVM.cmake
%ROOT_DIR%\winbuild\patch\bin\patch -f -p1 < %ROOT_DIR%\winbuild\osl_patch_1.diff
%ROOT_DIR%\winbuild\patch\bin\patch -f -p1 < %ROOT_DIR%\winbuild\osl_patch_2.diff

cd %WORKING_DIR%

mkdir %BUILD_DIR%\doc\licenses
copy LICENSE %BUILD_DIR%\doc\licenses\osl

rem We need to have the lib dir
set PATH=%PATH%;%BUILD_DIR%\lib;%BUILD_DIR%\bin;%ROOT_DIR%\winbuild\FlexBison\bin;%ROOT_DIR%\OpenShadingLanguage-Release-%OSL_VERSION%\gafferBuild\src\liboslcomp\Release

mkdir gafferBuild
cd gafferBuild

del /f CMakeCache.txt

cmake -Wno-dev -G %CMAKE_GENERATOR% -DCMAKE_BUILD_TYPE=%BUILD_TYPE% -DCMAKE_INSTALL_PREFIX=%BUILD_DIR% -DBUILDSTATIC=OFF -DUSE_OIIO_STATIC=OFF -DOSL_BUILD_PLUGINS=OFF -DENABLERTTI=1 -DCMAKE_PREFIX_PATH=%BUILD_DIR% -DSTOP_ON_WARNING=0 -DLLVM_DIRECTORY=%BUILD_DIR% -DUSE_LLVM_BITCODE=OFF -DOSL_BUILD_TESTS=OFF -DBOOST_ROOT=%BUILD_DIR% -DBoost_USE_STATIC_LIBS=OFF -DLLVM_STATIC=ON -DOPENEXR_INCLUDE_PATH=%BUILD_DIR%\\include -DOPENEXR_IMATH_LIBRARY=%BUILD_DIR%\\lib\\Imath-2_2.lib -DOPENEXR_ILMIMF_LIBRARY=%BUILD_DIR%\\lib\\IlmImf-2_2.lib -DOPENEXR_IEX_LIBRARY=%BUILD_DIR%\\lib\\Iex-2_2.lib -DOPENEXR_ILMTHREAD_LIBRARY=%BUILD_DIR%\\lib\\IlmThread-2_2.lib -DOPENIMAGEIOHOME=%BUILD_DIR% -DZLIB_INCLUDE_DIR=%BUILD_DIR%\\include -DZLIB_LIBRARY=%BUILD_DIR%\\lib\\zlib.lib -DEXTRA_CPP_ARGS="/DTINYFORMAT_ALLOW_WCHAR_STRINGS" -DFLEX_EXECUTABLE=flex -DBISON_EXECUTABLE=bison ..
if %ERRORLEVEL% NEQ 0 (exit /b %ERRORLEVEL%)
cmake --build . --config %BUILD_TYPE% --target install
if %ERRORLEVEL% NEQ 0 (exit /b %ERRORLEVEL%)

ENDLOCAL
