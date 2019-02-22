SETLOCAL

if not defined VERSION set VERSION=0.48.0.0

echo Building archive for version %VERSION%

cd %BUILD_DIR%
%ROOT_DIR%\winbuild\7zip\7za.exe a -tzip %ROOT_DIR%\gafferDependencies-%VERSION%-windows-msvc2017.zip @%ROOT_DIR%\winbuild\packageList.txt
if %ERRORLEVEL% NEQ 0 (exit /b %ERRORLEVEL%)

ENDLOCAL
