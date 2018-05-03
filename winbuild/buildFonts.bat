SETLOCAL

cd %ROOT_DIR%\ttf-bitstream-vera-1.10

mkdir %BUILD_DIR%\fonts
copy *.ttf %BUILD_DIR%\fonts

ENDLOCAL