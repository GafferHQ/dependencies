#!/bin/bash

set -e

cd `dirname $0`/../gafferResources-0.28.0.0

cp -r resources $BUILD_DIR
