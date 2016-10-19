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

- Make
- CMake
- SCons
- libbz2 (and headers)

### Invoking the build

The build is controlled by several environment variables, which must be set up before running :

- BUILD_DIR : Path to a directory where the build will be performed
- VERSION : Version number for the build - e.g. 0.29.0.0
- ARNOLD_ROOT : Path to the root of an Arnold installation
- RMAN_ROOT : Path to the root of a 3delight installation

The build is then initiated using a bash script, which should be run from the root directory of the project :

```
./build/buildAll.sh
```

Dependencies can be built individually using the other scripts in the build directory, for instance TBB is built as follows :

```
./build/buildTBB.sh
```
