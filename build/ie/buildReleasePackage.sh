#!/bin/bash
# This script builds the release package in the Image Engine environment.
# It should be run from a CentOS 6 machine.

set -e

# Simplify path to ignore IE wrappers.
export PATH=/software/apps/scons/2.0.1/bin/:/software/apps/doxygen/1.8.4/cent6.x86_64/bin:/software/apps/cmake/2.8.4/cent6.x86_64/bin:/usr/local/bin:/usr/bin:/bin

export RMAN_ROOT=/software/apps/3delight/12.0.25/cent6.x86_64
export ARNOLD_ROOT=/software/apps/arnold/4.1.2.0/cent6.x86_64

export BUILD_DIR=$HOME/gafferDependencies/build-$VERSION

`dirname $0`/../buildAll.sh
`dirname $0`/../buildPackage.sh
