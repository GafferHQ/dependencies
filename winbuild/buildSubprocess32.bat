cd %~dp0%..\subprocess32-3.2.6

copy subprocess32.py %BUILD_DIR%\lib64\site-packages\subprocess32.py
move %BUILD_DIR%\lib\python2.7\site-packages\iex.pyd %BUILD_DIR%\python
copy subprocess32.py %BUILD_DIR%\python\subprocess32.py
move %BUILD_DIR%\lib\python2.7\site-packages\iex.pyd %BUILD_DIR%\python

cd %ROOT_DIR%
