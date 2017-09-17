cd %~dp0%..\PyOpenGL-3.0.2

rem We need to have the lib dir
set BACKUP_PATH=%PATH%
set PATH=%PATH%;%BUILD_DIR%\\lib

set PYTHONPATH=%PYTHONPATH%;%BUILD_DIR%\\python

%BUILD_DIR%\bin\python.exe setup.py install --prefix %BUILD_DIR% --install-lib %BUILD_DIR%\python

rem Restore path
set PATH=%BACKUP_PATH%

cd %ROOT_DIR%
