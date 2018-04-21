cd %ROOT_DIR%\pyside-setup-6d8dee0

set SAVEPATH=%PATH%
set PATH=%BUILD_DIR%\bin;%PATH%
set PYTHONHOME=%BUILD_DIR%
set PYTHONPATH=%BUILD_DIR%\python;%PYTHONPATH%
rem Pyside will grab "version" from the environment if it exists and get
rem confused with the --ignore-git flag, so unset VERSION temporarily
set OLDVERSION=%VERSION%
set VERSION=

python setup.py --ignore-git --qmake=%BUILD_DIR%\bin\qmake.exe --openssl=%BUILD_DIR%\lib --cmake="C:\Program Files\CMake\bin\cmake.exe" install
if %ERRORLEVEL% NEQ 0 (exit /b %ERRORLEVEL%)

set PATH=%SAVEPATH%
set VERSION=%OLDVERSION%

