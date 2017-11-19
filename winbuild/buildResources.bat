cd %~dp0%..\gafferResources-0.28.0.0

mkdir %BUILD_DIR%\resources
xcopy /s resources %BUILD_DIR%\resources\

cd %ROOT_DIR%
