SETLOCAL

cd %ROOT_DIR%\PyOpenGL-3.0.2

rem We need to have the lib dir
set PATH=%PATH%;%BUILD_DIR%\\lib

set PYTHONPATH=%PYTHONPATH%;%BUILD_DIR%\\python

%BUILD_DIR%\bin\python.exe setup.py install --prefix %BUILD_DIR% --install-lib %BUILD_DIR%\python
if %ERRORLEVEL% NEQ 0 (exit /b %ERRORLEVEL%)

ENDLOCAL