cd %~dp0%..\boost_1_61_0

mkdir %BUILD_DIR%\doc\licenses
copy LICENSE_1_0.txt %BUILD_DIR%\doc\licenses\boost

rem give a helping hand to find the python headers, since the bootstrap below doesn't
rem always seem to get it right.
set CPLUS_INCLUDE_PATH=%BUILD_DIR%\\include\\python2.7;%BUILD_DIR%\\include

call bootstrap.bat --prefix=%BUILD_DIR% --with-python=%BUILD_DIR%\bin\python --with-python-root=%BUILD_DIR% --without-libraries=log
b2 --prefix=%BUILD_DIR% --toolset=%BOOST_MSVC_VERSION% architecture=x86 address-model=64 --build-type=complete variant=release link=shared threading=multi -s ZLIB_SOURCE=%ROOT_DIR%\..\zlib-1.2.11 -s ZLIB_INCLUDE=%BUILD_DIR%\include -s ZLIB_LIBPATH=%BUILD_DIR%\lib -s ZLIB_BINARY=zlib install

cd %ROOT_DIR%
