#!/bin/bash

set -e

cd `dirname $0`/../pyside-setup-6d8dee0

export LD_LIBRARY_PATH=$BUILD_DIR/lib${LD_LIBRARY_PATH:+:}${LD_LIBRARY_PATH:-}
export PATH=$BUILD_DIR/bin:$PATH
export MACOSX_DEPLOYMENT_TARGET=10.9

# If you need to hack on the PySide source itself, you might like to
# know that you can use the `--reuse-build` argument to stop `setup.py`
# from rebuilding the entire goddam thing each time.
python setup.py --ignore-git --jobs `getconf _NPROCESSORS_ONLN` --osx-use-libc++ install
