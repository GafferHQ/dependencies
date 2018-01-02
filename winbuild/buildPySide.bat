cd %~dp0%..\pyside-setup-6d8dee0

rem set SAVEPATH=%PATH%
rem set PATH=%BUILD_DIR%\bin;%PATH%
rem set PYTHONHOME=%BUILD_DIR%
rem set PYTHONPATH=%BUILD_DIR%\python;%PYTHONPATH%

python setup.py --ignore-git --qmake=%BUILD_DIR%\bin\qmake.exe --openssl=%BUILD_DIR%\lib --cmake="C:\Program Files\CMake\bin\cmake.exe" install

rem set PATH=%SAVEPATH%

cd %ROOT_DIR%
