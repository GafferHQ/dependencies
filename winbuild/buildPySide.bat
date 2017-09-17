cd %~dp0%..\pyside-setup-6d8dee0

python setup.py --ignore-git --qmake=%BUILD_DIR%\bin\qmake.exe --openssl=%BUILD_DIR%\lib --cmake="C:\Program Files\CMake\bin\cmake.exe" install

cd %ROOT_DIR%
