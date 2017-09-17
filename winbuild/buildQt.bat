cd %~dp0%..\qt-adsk-5.6.1

mkdir %BUILD_DIR%\doc\licenses
copy LICENSE.LGPLv21 %BUILD_DIR%\doc\licenses\qt

rem Qt5 wants 'zdll.lib' not 'zlib.lib'
copy %BUILD_DIR%\lib\zlib.lib %BUILD_DIR%\lib\zdll.lib
rem Qt5 wants 'libpng.lib' not 'libpng16.lib'
copy %BUILD_DIR%\lib\libpng16.lib %BUILD_DIR%\lib\libpng.lib
rem Qt5 wants 'libjpeg.lib' not 'jpeg.lib'
copy %BUILD_DIR%\lib\jpeg.lib %BUILD_DIR%\lib\libjpeg.lib

rem We need to have the lib dir
set BACKUP_PATH=%PATH%
set PATH=%PATH%;%BUILD_DIR%\\lib;%BUILD_DIR%\\bin

jom\jom.exe distclean
call configure.bat -prefix %BUILD_DIR% -plugindir %BUILD_DIR%\qt\plugins -release -opensource -confirm-license -opengl desktop -no-angle -no-audio-backend -no-dbus -skip qtconnectivity -skip qtwebengine -skip qt3d -skip qtdeclarative -skip qtwebkit -nomake examples -nomake tests -system-zlib -no-openssl -I %BUILD_DIR%\include -L %BUILD_DIR%\lib

jom\jom.exe
jom\jom.exe install

rem Restore path
set PATH=%BACKUP_PATH%

cd %ROOT_DIR%
