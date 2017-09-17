
cd %~dp0%..\tbb43_20150611oss

mkdir %BUILD_DIR%\doc\licenses
copy COPYING %BUILD_DIR%\doc\licenses\tbb

rmdir /s /q build\vs2013\intel64
msbuild build\vs2013\makefile.sln /t:Build /p:Configuration="Release-MT" /p:Platform=x64

mkdir %BUILD_DIR%\include\tbb
xcopy /E /Q /Y include\tbb %BUILD_DIR%\include\tbb
copy build\vs2013\intel64\Release-MT\tbb.dll %BUILD_DIR%\lib
copy build\vs2013\intel64\Release-MT\tbbmalloc.dll %BUILD_DIR%\lib
copy build\vs2013\intel64\Release-MT\tbbmalloc_proxy.dll %BUILD_DIR%\lib
copy build\vs2013\intel64\Release-MT\tbb.lib %BUILD_DIR%\lib
copy build\vs2013\intel64\Release-MT\tbbmalloc.lib %BUILD_DIR%\lib
copy build\vs2013\intel64\Release-MT\tbbmalloc_proxy.lib %BUILD_DIR%\lib

cd %ROOT_DIR%
