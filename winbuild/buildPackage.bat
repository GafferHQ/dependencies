cd %BUILD_DIR%
%ROOT_DIR%\winbuild\7zip\7za.exe a -tzip %ROOT_DIR%\gafferDependencies-0.42.0.0-windows-msvc2017.zip @%ROOT_DIR%\winbuild\packageList.txt
if %ERRORLEVEL% NEQ 0 (exit /b %ERRORLEVEL%)
