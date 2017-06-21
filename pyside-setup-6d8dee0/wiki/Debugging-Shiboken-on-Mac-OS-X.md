# Debugging Shiboken on Mac OS X

Sometimes, you might want to debug shiboken's execution, in order to find something out.
I had to do that several times to fix hard-to-find bugs.

Because PySide2's build process is pretty complex, I was afraid how difficult this was to configure, but actually, I found the following simple steps:

### set up an XCode project via cmake (sic!)
 
Let us assume the following folder structure:

The top level folder is `pyside2-setup`. Create a subfolder and use cmake to create an xcode project:
```
$ cd pyside2-setup
$ mkdir shibug
$ cd shibug
$ cmake ../sources/shiboken2/ -G Xcode  -DQt5_DIR=/usr/local/Cellar/qt5/5.5.0/lib/cmake/Qt5 \
 -DQt5Xml_DIR=/usr/local/Cellar/qt5/5.5.0/lib/cmake/Qt5Xml \
 -DQt5XmlPatterns_DIR=/usr/local/Cellar/qt5/5.5.0/lib/cmake/Qt5XmlPatterns
 -DCMAKE_OSX_DEPLOYMENT_TARGET=10.7
```
After that, you have `shibiken2.xcodeproj` that you can double-click, and the project will open in XCode.
You then need to make a few adjustments to the project, like setting the deployment target if you forgot that
in the command line.

When you are asked by XCode, if you want to automatically create targets, refuse to do this. You only need to select
the single `shiboken` target. Everything dependent will be built for you. Before building,

- open the project navigator
- select the shiboken project
- in the info menu you find 'OS X Deployment Target'. Select '10.7'.

Now you can click the triangle button on the left top and build the first time.

### The second part of the trick is about what you need to enter as parameters:

Instead of tedious work on finding the parameters, you can simply use what was already generated for you. In my last debug session, I wanted to single-step through the actions of shiboken, when it generates the wrapper code for QtCore. By using the generated files from your last build, this is pretty straightforward.

From my `pyside2-setup` folder, I start a quick build:
```
$ python3 setup.py build --debug --no-examples --ignore-git --qmake=/usr/local/Cellar/qt5/5.5.0/bin/qmake --jobs=9
```
After that ran a bit, you find this generated file:
```
# /Users/tismer/src/pyside2-setup/pyside_build/py3.4-qt5.5.0-64bit-debug/pyside/PySide/QtCore/CMakeFiles/QtCore.dir/build.make
```
In that file, you find the shiboken arguments that were used.

```
PySide/QtCore/PySide/QtCore/qabstractanimation_wrapper.cpp:
    @$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --blue --bold --progress-dir=/Users/tismer/src/pyside2-setup/pyside_build/py3.4-qt5.5.0-64bit-debug/pyside/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Running generator for QtCore..."
    cd /Users/tismer/src/pyside2-setup/sources/pyside2/PySide/QtCore && /Users/tismer/src/pyside2-setup/pyside_install/py3.4-qt5.5.0-64bit-debug/bin/shiboken --generator-set=shiboken --enable-parent-ctor-heuristic --enable-pyside-extensions --enable-return-value-heuristic --use-isnull-as-nb_nonzero /Users/tismer/src/pyside2-setup/pyside_build/py3.4-qt5.5.0-64bit-debug/pyside/PySide/pyside_global.h --include-paths=/Users/tismer/src/pyside2-setup/sources/pyside2/PySide:/usr/local/Cellar/qt5/5.5.0/include --typesystem-paths=/Users/tismer/src/pyside2-setup/sources/pyside2/PySide:/Users/tismer/src/pyside2-setup/sources/pyside2/PySide/QtCore --output-directory=/Users/tismer/src/pyside2-setup/pyside_build/py3.4-qt5.5.0-64bit-debug/pyside/PySide/QtCore --license-file=/Users/tismer/src/pyside2-setup/sources/pyside2/PySide/QtCore/../licensecomment.txt /Users/tismer/src/pyside2-setup/pyside_build/py3.4-qt5.5.0-64bit-debug/pyside/PySide/QtCore/typesystem_core.xml --api-version=5.5 --drop-type-entries=""
```
Don't try to scroll through this super-long line. Instead, use a good editor (I'm a sublime text user) and replace every blank with a newline. After stripping away the shiboken command itself, you end up with something like this:
```
--generator-set=shiboken
--enable-parent-ctor-heuristic
--enable-pyside-extensions
--enable-return-value-heuristic
--use-isnull-as-nb_nonzero
/Users/tismer/src/pyside2-setup/pyside_build/py3.4-qt5.5.0-64bit-debug/pyside/PySide/pyside_global.h
--include-paths=/Users/tismer/src/pyside2-setup/sources/pyside2/PySide:/usr/local/Cellar/qt5/5.5.0/include
--typesystem-paths=/Users/tismer/src/pyside2-setup/sources/pyside2/PySide:/Users/tismer/src/pyside2-setup/sources/pyside2/PySide/QtCore
--output-directory=/Users/tismer/src/pyside2-setup/pyside_build/py3.4-qt5.5.0-64bit-debug/pyside/PySide/QtCore
--license-file=/Users/tismer/src/pyside2-setup/sources/pyside2/PySide/licensecomment.txt
/Users/tismer/src/pyside2-setup/sources/pyside2/PySide/QtCore/typesystem_core.xml
--api-version=5.5
--drop-type-entries=""
```
You need to change the newlines back into spaces and paste these arguments into the parameters of your xcode project. Now start the program. You should see some output of messages in the XCode window, that you should know from the console.

Now you are all set. You can set breakpoints and find out what happens in shiboken.

Caveat: I found no good way to inspect the values of strings, because shiboken insists to use the QString construct.

XXX add hint how to print debugging text XXX