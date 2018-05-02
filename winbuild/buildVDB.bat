SETLOCAL

cd %ROOT_DIR%\openvdb-4.0.2

mkdir %BUILD_DIR%\doc\licenses
copy openvdb\LICENSE %BUILD_DIR%\doc\licenses\openvdb

mkdir gafferBuild
cd gafferBuild

del /f CMakeCache.txt

cmake -Wno-dev -G %CMAKE_GENERATOR% -DCMAKE_BUILD_TYPE=%BUILD_TYPE% -DCMAKE_INSTALL_PREFIX=%BUILD_DIR% -DOPENVDB_BUILD_UNITTESTS=OFF -DOPENVDB_BUILD_DOCS=OFF -DOPENVDB_BUILD_PYTHON_MODULE=ON -DUSE_GLFW3=OFF -DTBB_LOCATION=%BUILD_DIR% -DBOOST_ROOT=%BUILD_DIR% -DGLEW_LOCATION=%BUILD_DIR% -DILMBASE_LOCATION=%BUILD_DIR% -DOPENEXR_LOCATION=%BUILD_DIR% -DBLOSC_LOCATION=%BUILD_DIR% -DOPENVDB_ENABLE_3_ABI_COMPATIBLE=OFF ..
if %ERRORLEVEL% NEQ 0 (exit /b %ERRORLEVEL%)
cmake --build . --config %BUILD_TYPE% --target install
if %ERRORLEVEL% NEQ 0 (exit /b %ERRORLEVEL%)

rem not sure why this doesn't get put in the site-packages directory, but move it to they python directory regardless
move %BUILD_DIR%\lib\python2.7\pyopenvdb.pyd %BUILD_DIR%\python
if %ERRORLEVEL% NEQ 0 (exit /b %ERRORLEVEL%)

ENDLOCAL