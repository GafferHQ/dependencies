#!/bin/bash

# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Sets up the appropriate environment for Visual Studio 2013 command line
# development. Assumes the toolchain has been installed via depot_tools.
# The environment settings only persist while the script is executing. The
# command argument must be supplied to be run by this script while the
# environment is still configured.

if [ "$#" -lt 3 ]; then
  echo "Usage: $(basename $0) /path/to/depot_tools arch command"
  echo "    arch     must be either x86 or x64"
  echo "    command  command to execute after environment is configured"
  exit 1
fi

if [ ! -d $1 ]; then
  echo "Directory does not exist: $1"
  exit 1
fi

VSPATH=$1/win_toolchain/vs2013_files

if [ ! -d $VSPATH ]; then
  echo "Visual Studio 2013 toolchain not found: $VSPATH"
  echo "Please see http://www.chromium.org/developers/how-tos/build-instructions-windows"
  echo
  echo "It's also possible that we've upgraded past 2013, in which case please send"
  echo "out a patch updating this script."
  exit 1
fi

function add_path {
  if [ ! -d "$1" ]; then
    echo "Cannot add '$1' to path; directory does not exist." >&2
    exit 1
  fi
  if [ -z "$path" ]; then
    path="$1"
    return
  fi
  path="$path:$1"
}

function add_include_path {
  if [ ! -d "$1" ]; then
    echo "Cannot add '$1' to include path; directory does not exist." >&2
    exit 1
  fi
  if [ -z "$include" ]; then
    include="$(cygpath -w $1)"
    return
  fi
  include="$include;$(cygpath -w $1)"
}

function add_lib_path {
  if [ ! -d "$1" ]; then
    echo "Cannot add '$1' to lib path; directory does not exist." >&2
    exit 1
  fi
  if [ -z "$lib" ]; then
    lib="$(cygpath -w $1)"
    return
  fi
  lib="$lib;$(cygpath -w $1)"
}

case "$2" in
  "x86")
    add_path $VSPATH/win8sdk/bin/x86
    add_path $VSPATH/VC/bin/amd64_x86
    add_path $VSPATH/VC/bin/amd64

    add_lib_path $VSPATH/VC/lib
    add_lib_path $VSPATH/win8sdk/Lib/winv6.3/um/x86
    add_lib_path $VSPATH/VC/atlmfc/lib
    ;;

  "x64")
    add_path $VSPATH/win8sdk/bin/x64
    add_path $VSPATH/VC/bin/amd64

    add_lib_path $VSPATH/VC/lib/amd64
    add_lib_path $VSPATH/win8sdk/Lib/winv6.3/um/x64
    add_lib_path $VSPATH/VC/atlmfc/lib/amd64
    ;;

  *)
    echo "Unknown architecture: $2"
    exit 1
    ;;
esac

# Common for x86 and x64.
add_path $(dirname $(readlink -f "$0")) # For cygwin-wrapper.
add_include_path $VSPATH/win8sdk/Include/um
add_include_path $VSPATH/win8sdk/Include/shared
add_include_path $VSPATH/VC/include
add_include_path $VSPATH/VC/atlmfc/include

export PATH=$path:$PATH
export INCLUDE=$include
export LIB=$lib

# Now execute whatever is left trailing.
shift
shift
"$@"
