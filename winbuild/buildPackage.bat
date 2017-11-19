cd %BUILD_DIR%
%ROOT_DIR%\7zip\7za.exe a -tzip ..\gafferDependencies-0.41.0.0-windows-msvc2013.zip @%ROOT_DIR%\packageList.txt
cd %ROOT_DIR%