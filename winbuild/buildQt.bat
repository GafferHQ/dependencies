SETLOCAL

cd %ROOT_DIR%\qt-adsk-5.6.1

mkdir %BUILD_DIR%\doc\licenses
copy LICENSE.LGPLv21 %BUILD_DIR%\doc\licenses\qt

rem Qt5 wants 'zdll.lib' not 'zlib.lib'
copy %BUILD_DIR%\lib\zlib.lib %BUILD_DIR%\lib\zdll.lib
rem Qt5 wants 'libpng.lib' not 'libpng16.lib'
copy %BUILD_DIR%\lib\libpng16.lib %BUILD_DIR%\lib\libpng.lib
rem Qt5 wants 'libjpeg.lib' not 'jpeg.lib'
copy %BUILD_DIR%\lib\jpeg.lib %BUILD_DIR%\lib\libjpeg.lib

rem We need to have the lib dir
set PATH=%PATH%;%BUILD_DIR%\\lib;%BUILD_DIR%\\bin

rem We should probably check this batch file to make sure ERRORLEVEL
rem is set appropriately?

%ROOT_DIR%\winbuild\jom\jom.exe distclean
call configure.bat -prefix %BUILD_DIR% -plugindir %BUILD_DIR%\qt\plugins -release -opensource -confirm-license -opengl desktop -no-angle -no-audio-backend -no-dbus -skip qtconnectivity -skip qtwebengine -skip qt3d -skip qtdeclarative -skip qtwebkit -nomake examples -nomake tests -system-zlib -no-openssl -I %BUILD_DIR%\include -L %BUILD_DIR%\lib
if %ERRORLEVEL% NEQ 0 (exit /b %ERRORLEVEL%)

%ROOT_DIR%\winbuild\jom\jom.exe
if %ERRORLEVEL% NEQ 0 (exit /b %ERRORLEVEL%)
%ROOT_DIR%\winbuild\jom\jom.exe install
if %ERRORLEVEL% NEQ 0 (exit /b %ERRORLEVEL%)

ENDLOCAL