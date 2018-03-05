cd %~dp0%..\pyside-setup-6d8dee0

set SAVEPATH=%PATH%
set PATH=%BUILD_DIR%\bin;%PATH%
set PYTHONHOME=%BUILD_DIR%
set PYTHONPATH=%BUILD_DIR%\python;%PYTHONPATH%

python setup.py --ignore-git --qmake=%BUILD_DIR%\bin\qmake.exe --openssl=%BUILD_DIR%\lib --cmake="C:\Program Files\CMake\bin\cmake.exe" install

set PATH=%SAVEPATH%

cd %ROOT_DIR%
