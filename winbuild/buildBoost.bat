SETLOCAL

cd %ROOT_DIR%\boost_1_61_0

mkdir %BUILD_DIR%\doc\licenses
copy LICENSE_1_0.txt %BUILD_DIR%\doc\licenses\boost

set PYTHONPATH=%BUILD_DIR%;%BUILD_DIR%\bin;%BUILD_DIR%\lib64;%BUILD_DIR%\lib
set PATH=%BUILD_DIR%\bin;%PATH%

call bootstrap.bat --prefix=%BUILD_DIR% --with-python=%BUILD_DIR% --with-python-root=%BUILD_DIR% --without-libraries=log
if %ERRORLEVEL% NEQ 0 (exit /b %ERRORLEVEL%)
set "BUILD_DIR=%BUILD_DIR:\=\\%"
echo "using python ^: 2.7 : %BUILD_DIR% ^: %BUILD_DIR%\include ^: %BUILD_DIR%\lib ^: ^<address-model^>64 ^; >> project-config.jam"
b2 --prefix=%BUILD_DIR% --toolset=%BOOST_MSVC_VERSION% architecture=x86 address-model=64 --build-type=complete variant=release link=shared threading=multi -s ZLIB_SOURCE=%ROOT_DIR%\zlib-1.2.11 -s ZLIB_INCLUDE=%BUILD_DIR%\include -s ZLIB_LIBPATH=%BUILD_DIR%\lib -s ZLIB_BINARY=zlib install
if %ERRORLEVEL% NEQ 0 (exit /b %ERRORLEVEL%)
set "BUILD_DIR=%BUILD_DIR:\\=\%"

ENDLOCAL
