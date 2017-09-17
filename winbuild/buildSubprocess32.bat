cd %~dp0%..\subprocess32-3.2.6

rem ensure that we use the right python to do the install
set PATH=%BUILD_DIR%\bin;%BUILD_DIR%\lib;%PATH%

python setup.py install

cd %ROOT_DIR%