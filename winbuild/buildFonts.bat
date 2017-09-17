cd %~dp0%..\ttf-bitstream-vera-1.10

mkdir %BUILD_DIR%\fonts
copy *.ttf %BUILD_DIR%\fonts

cd %ROOT_DIR%
