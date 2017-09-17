cd %~dp0%..\tiff-3.8.2

mkdir %BUILD_DIR%\doc\licenses
copy COPYRIGHT %BUILD_DIR%\doc\licenses\libtiff

nmake /f makefile.vc

copy libtiff\libtiff.lib %BUILD_DIR%\lib
copy libtiff\*.h %BUILD_DIR%\include

cd %ROOT_DIR%
