#!/bin/bash

set -e

for n in "BUILD_DIR" "ARNOLD_ROOT" "RMAN_ROOT" ; do
	if [ -z "${!n}" ]; then
		echo "ERROR : $n environment variable not set"
		exit 1
	fi
done

if [[ `uname` = "Darwin" ]] ; then
	if csrutil status | grep -q enabled ; then
		# SIP prevents DYLD_* environment variables
		# being inherited by protected processes, which
		# breaks the builds for OSL, PySide and Appleseed.
		# These each try to run a binary they create during
		# the build, and cmake/make run that binary via a
		# shell which throws away the library path, preventing
		# runtime linking from working.
		echo "ERROR : SIP not disabled"
		exit 1
	fi
fi

export MACOSX_DEPLOYMENT_TARGET=10.12

cd `dirname $0`
(cd .. && ./build/build.py --buildDir $BUILD_DIR)
./buildPackage.sh
