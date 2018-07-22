Gaffer Dependencies
===================

This project contains the source code for all the 3rd party libraries required to build [Gaffer](https://github.com/GafferHQ/gaffer). It is intended to help Gaffer developers get up and running as quickly as possible, and secondarily to provide a record of exactly which dependencies were used for each release of Gaffer, and how they were built.

Usage
-----

We provide prebuilt binary [releases](https://github.com/GafferHQ/dependencies/releases), which should be used in conjunction with Gaffer's [build instructions](https://github.com/GafferHQ/gaffer#building).

Building
--------

We strongly recommend using the precompiled binaries as described above, but if you really like building multiple huge software projects, read on!

### Prerequisites

Since the build covers so many different projects, it requires the installation of several common software development tools, most of which should be installed by default on a typical developer machine. They include (but are probably not limited to) :

#### Linux and OS X
- Make
- CMake
- SCons
- libbz2 (and headers)

#### Windows
- Microsoft Visual Studio 2017 (Community edition is a free version that is known to work)
- CMake
- Python 2.7.xx

### Invoking the build

The build is controlled by several environment variables, which must be set up before running :

- BUILD_DIR : Path to a directory where the build will be performed
- ARNOLD_ROOT : Path to the root of an Arnold installation
- RMAN_ROOT : Path to the root of a 3delight installation

#### Linux and OS X
The build is then initiated using a bash script, which should be run from the root directory of the project :

```
./build/buildAll.sh
```

Dependencies can be built individually using the other scripts in the build directory, for instance TBB is built as follows :

```
./build/buildTBB.sh
```
#### Windows
The build is then initiated using the "x64 Native Tools Command Prompt" for Visual Studio 2017. This is a special command prompt that sets up all of the environment variables for Visual Studio to find the various tools needed for building. It is installed with Visual Studio and is accessible from the start menu under Visual Studio 2017.

To build all of the dependencies run buildAll.bat from the winbuild directory:
```
./winbuild/buildAll.bat
```

Dependencies can be built individually using other batch files in the build directory, for instance Python can be built as follows:
```
./winbuild/buildPython.bat
```

#### Universal Python Builder
Some packages are built using a common Python script and configuration settings for each package. These are built by calling the build.py script in the "build" directory (for all of Linux, OS X and Windows) and specifying the package to build and the build directory.

For example, to build Boost on Linux and OS X and you would run:
```
./build/build.py --project Boost --buildDir $BUILD_DIR
```
and on Windows :
```
python build/build.py --project Boost --buildDir %BUILD_DIR%
```
A list of projects that have are built using this script is shown if you run
```
./build/build.py
```