SETLOCAL

cd %ROOT_DIR%\pyside-setup-6d8dee0

set PATH=%BUILD_DIR%\bin;%PATH%
set PYTHONHOME=%BUILD_DIR%
set PYTHONPATH=%BUILD_DIR%\python;%PYTHONPATH%
rem Pyside will grab "version" from the environment if it exists and get
rem confused with the --ignore-git flag, so unset VERSION temporarily
set VERSION=

python setup.py --ignore-git --qmake=%BUILD_DIR%\bin\qmake.exe --openssl=%BUILD_DIR%\lib --cmake="C:\Program Files\CMake\bin\cmake.exe" install
if %ERRORLEVEL% NEQ 0 (exit /b %ERRORLEVEL%)
REM pyside2 gets installed in the lib\site-packages directory for some reason, move it to python
xcopy /e /q /y %BUILD_DIR%\lib\site-packages\* %BUILD_DIR%\python

ENDLOCAL