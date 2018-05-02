SETLOCAL

cd %ROOT_DIR%\subprocess32-3.2.6

copy subprocess32.py %BUILD_DIR%\lib64\site-packages\subprocess32.py
mkdir %BUILD_DIR%\python
copy subprocess32.py %BUILD_DIR%\python\subprocess32.py
if %ERRORLEVEL% NEQ 0 (exit /b %ERRORLEVEL%)

ENDLOCAL