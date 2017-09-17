cd %~dp0%..\xerces-c-3.1.2

mkdir %BUILD_DIR%\doc\licenses
copy LICENSE %BUILD_DIR%\doc\licenses\xerces

pushd projects\Win32\VC12.gaffer\xerces-all

devenv xerces-all.sln /build "Static Release" /project all

popd

xcopy /E /Q /Y src\*.hpp %BUILD_DIR%\include\
xcopy /E /Q /Y src\*.c %BUILD_DIR%\include\
copy "Build\Win64\VC12\Static Release\xerces-c_static_3.lib" %BUILD_DIR%\lib

cd %ROOT_DIR%
