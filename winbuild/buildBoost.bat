cd %~dp0%..\boost_1_61_0

mkdir %BUILD_DIR%\doc\licenses
copy LICENSE_1_0.txt %BUILD_DIR%\doc\licenses\boost

set PYTHONPATH=%BUILD_DIR%;%BUILD_DIR%\bin;%BUILD_DIR%\lib64;%BUILD_DIR%\lib
set SAVEPATH=%PATH%
set PATH=%BUILD_DIR%\bin;%PATH%

call bootstrap.bat --prefix=%BUILD_DIR% --with-python=%BUILD_DIR% --with-python-root=%BUILD_DIR% --without-libraries=log
set "BUILD_DIR=%BUILD_DIR:\=\\%"
echo using python ^: ^2.7 : %BUILD_DIR% ^: %BUILD_DIR%\\include ^: %BUILD_DIR%\\lib ^: ^<address-model^>64 ^; >> project-config.jam
b2 --prefix=%BUILD_DIR% --toolset=%BOOST_MSVC_VERSION% architecture=x86 address-model=64 --build-type=complete variant=release link=shared threading=multi -s ZLIB_SOURCE=%ROOT_DIR%\..\zlib-1.2.11 -s ZLIB_INCLUDE=%BUILD_DIR%\include -s ZLIB_LIBPATH=%BUILD_DIR%\lib -s ZLIB_BINARY=zlib install
set "BUILD_DIR=%BUILD_DIR:\\=\%"

cd %ROOT_DIR%

set PATH=%SAVEPATH%
