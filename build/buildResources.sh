#!/bin/bash

set -e

cd `dirname $0`/../gafferResources-0.27.0.0

cp -r resources $BUILD_DIR
