#!/bin/bash
###############################################################################
# Copyright 2015 Google Inc.
#
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
###############################################################################
#
# android_setup.sh: Sets environment variables used by other Android scripts.

# Fail-fast if anything in the script fails.
set -e

BUILDTYPE=${BUILDTYPE-Debug}

while (( "$#" )); do
  if [[ "$1" == "-d" ]]; then
    DEVICE_ID=$2
    shift
  elif [[ "$1" == "-i" || "$1" == "--resourcePath" ]]; then
    RESOURCE_PATH=$2
    APP_ARGS=("${APP_ARGS[@]}" "${1}" "${2}")
    shift
  elif [[ "$1" == "-s" ]]; then
    DEVICE_SERIAL="-s $2"
    shift
  elif [[ "$1" == "-t" ]]; then
    BUILDTYPE=$2
    shift
  elif [[ "$1" == "--release" ]]; then
    BUILDTYPE=Release
  elif [[ "$1" == "--clang" ]]; then
    USE_CLANG="true"
    export GYP_DEFINES="skia_clang_build=1 $GYP_DEFINES"
  elif [[ "$1" == "--logcat" ]]; then
    LOGCAT=1
  elif [[ "$1" == "--verbose" ]]; then
    VERBOSE="true"
  else
    APP_ARGS=("${APP_ARGS[@]}" "${1}")
  fi
  shift
done

function verbose {
  if [[ -n $VERBOSE ]]; then
    echo $@
  fi
}

function exportVar {
  NAME=$1
  VALUE=$2
  verbose export $NAME=\"$VALUE\"
  export $NAME="$VALUE"
}

function absPath {
  (cd $1; pwd)
}

SCRIPT_DIR=$(absPath "$(dirname "$BASH_SOURCE[0]}")")

if [ -z "$ANDROID_SDK_ROOT" ]; then
  if ANDROID_SDK_ROOT="$(dirname $(which android))/.."; then
    exportVar ANDROID_SDK_ROOT $ANDROID_SDK_ROOT
  else
     echo "No ANDROID_SDK_ROOT set and can't auto detect it from location of android binary."
     exit 1
  fi
fi

# check to see that gclient sync ran successfully
THIRD_PARTY_EXTERNAL_DIR=${SCRIPT_DIR}/../third_party/externals
if [ ! -d "$THIRD_PARTY_EXTERNAL_DIR" ]; then
	echo ""
	echo "ERROR: Unable to find the required third_party dependencies needed to build."
	echo "       To fix this add the following line to your .gclient file and run 'gclient sync'"
	echo "        target_os = ['android']"
	echo ""
	exit 1;
fi

# Helper function to configure the GYP defines to the appropriate values
# based on the target device.
setup_device() {
  DEFINES="OS=android"
  DEFINES="${DEFINES} host_os=$(uname -s | sed -e 's/Linux/linux/;s/Darwin/mac/')"
  DEFINES="${DEFINES} skia_os=android"
  DEFINES="${DEFINES} android_base=$(absPath ${SCRIPT_DIR}/..)"
  if [[ "$GYP_DEFINES" != *skia_shared_lib=* ]]; then
      DEFINES="${DEFINES} skia_shared_lib=1"
  fi

  # Setup the build variation depending on the target device
  TARGET_DEVICE="$1"

  if [ -z "$TARGET_DEVICE" ]; then
    if [ -f .android_config ]; then
      TARGET_DEVICE=$(cat .android_config)
      verbose "no target device (-d), using ${TARGET_DEVICE} from most recent build"
    else
      TARGET_DEVICE="arm_v7_neon"
      verbose "no target device (-d), using ${TARGET_DEVICE}"
    fi
  fi

  case $TARGET_DEVICE in
    arm)
      DEFINES="${DEFINES} skia_arch_type=arm arm_neon=0"
      ANDROID_ARCH="arm"
      ;;
    arm_v7 | xoom)
      DEFINES="${DEFINES} skia_arch_type=arm arm_neon_optional=1 arm_version=7"
      ANDROID_ARCH="arm"
      ;;
    arm_v7_neon | nexus_4 | nexus_5 | nexus_6 | nexus_7 | nexus_10)
      DEFINES="${DEFINES} skia_arch_type=arm arm_neon=1 arm_version=7"
      ANDROID_ARCH="arm"
      ;;
    arm64 | nexus_9)
      DEFINES="${DEFINES} skia_arch_type=arm64 skia_arch_width=64"
      ANDROID_ARCH="arm64"
      ;;
    x86)
      DEFINES="${DEFINES} skia_arch_type=x86"
      ANDROID_ARCH="x86"
      ;;
    x86_64 | x64)
      DEFINES="${DEFINES} skia_arch_type=x86_64"
      ANDROID_ARCH="x86_64"
      ;;
    mips)
      DEFINES="${DEFINES} skia_arch_type=mips skia_arch_width=32"
      DEFINES="${DEFINES} skia_resource_cache_mb_limit=32"
      ANDROID_ARCH="mips"
      ;;
    mips_dsp2)
      DEFINES="${DEFINES} skia_arch_type=mips skia_arch_width=32"
      DEFINES="${DEFINES} mips_arch_variant=mips32r2 mips_dsp=2"
      ANDROID_ARCH="mips"
      ;;
    mips64)
      DEFINES="${DEFINES} skia_arch_type=mips skia_arch_width=64"
      ANDROID_ARCH="mips64"
      ;;
    *)
      if [ -z "$ANDROID_IGNORE_UNKNOWN_DEVICE" ]; then
          echo "ERROR: unknown device $TARGET_DEVICE"
          exit 1
      fi
      # If ANDROID_IGNORE_UNKNOWN_DEVICE is set, then ANDROID_TOOLCHAIN
      # or ANDROID_ARCH should be set; Otherwise, ANDROID_ARCH
      # defaults to 'arm' and the default ARM toolchain is used.
      DEFINES="${DEFINES} skia_arch_type=${ANDROID_ARCH-arm}"
      # If ANDROID_IGNORE_UNKNOWN_DEVICE is set, extra gyp defines can be
      # added via ANDROID_GYP_DEFINES
      DEFINES="${DEFINES} ${ANDROID_GYP_DEFINES}"
      ;;
  esac

  verbose "The build is targeting the device: $TARGET_DEVICE"
  exportVar DEVICE_ID $TARGET_DEVICE

  # setup the appropriate cross compiling toolchains
  source $SCRIPT_DIR/utils/setup_toolchain.sh

  DEFINES="${DEFINES} android_toolchain=${TOOLCHAIN_TYPE}"
  exportVar GYP_DEFINES "$DEFINES $GYP_DEFINES"

  SKIA_SRC_DIR=$(cd "${SCRIPT_DIR}/../../.."; pwd)
  DEFAULT_SKIA_OUT="${SKIA_SRC_DIR}/out/config/android-${TARGET_DEVICE}"
  exportVar SKIA_OUT "${SKIA_OUT:-${DEFAULT_SKIA_OUT}}"
}

# adb_pull_if_needed(android_src, host_dst)
adb_pull_if_needed() {

  # get adb location
  source $SCRIPT_DIR/utils/setup_adb.sh

  # read input params
  ANDROID_SRC="$1"
  HOST_DST="$2"

  if [ -f $HOST_DST ];
  then
    #get the MD5 for dst and src depending on OS and/or OS revision
    ANDROID_MD5_SUPPORT=`$ADB $DEVICE_SERIAL shell ls -ld /system/bin/md5`
    if [ "${ANDROID_MD5_SUPPORT:0:15}" != "/system/bin/md5" ]; then
      ANDROID_MD5=`$ADB $DEVICE_SERIAL shell md5 $ANDROID_DST`
    else
      ANDROID_MD5=`$ADB $DEVICE_SERIAL shell md5sum $ANDROID_DST`
    fi
    if [ $(uname) == "Darwin" ]; then
      HOST_MD5=`md5 -q $HOST_DST`
    else
      HOST_MD5=`md5sum $HOST_DST`
    fi

    if [ "${ANDROID_MD5:0:32}" != "${HOST_MD5:0:32}" ]; then
      echo -n "$HOST_DST "
      $ADB $DEVICE_SERIAL pull $ANDROID_SRC $HOST_DST
    fi
  else
    echo -n "$HOST_DST "
    $ADB $DEVICE_SERIAL pull $ANDROID_SRC $HOST_DST
  fi
}

# adb_push_if_needed(host_src, android_dst)
adb_push_if_needed() {

  # get adb location
  source $SCRIPT_DIR/utils/setup_adb.sh

  # read input params
  local HOST_SRC="$1"
  local ANDROID_DST="$2"

  ANDROID_LS=`$ADB $DEVICE_SERIAL shell ls -ld $ANDROID_DST`
  HOST_LS=`ls -ld $HOST_SRC`
  if [ "${ANDROID_LS:0:1}" == "d" -a "${HOST_LS:0:1}" == "-" ];
  then
    ANDROID_DST="${ANDROID_DST}/$(basename ${HOST_SRC})"
  fi


  ANDROID_LS=`$ADB $DEVICE_SERIAL shell ls -ld $ANDROID_DST`
  if [ "${ANDROID_LS:0:1}" == "-" ]; then
    #get the MD5 for dst and src depending on OS and/or OS revision
    ANDROID_MD5_SUPPORT=`$ADB $DEVICE_SERIAL shell ls -ld /system/bin/md5`
    if [ "${ANDROID_MD5_SUPPORT:0:15}" != "/system/bin/md5" ]; then
      ANDROID_MD5=`$ADB $DEVICE_SERIAL shell md5 $ANDROID_DST`
    else
      ANDROID_MD5=`$ADB $DEVICE_SERIAL shell md5sum $ANDROID_DST`
    fi

    if [ $(uname) == "Darwin" ]; then
      HOST_MD5=`md5 -q $HOST_SRC`
    else
      HOST_MD5=`md5sum $HOST_SRC`
    fi

    if [ "${ANDROID_MD5:0:32}" != "${HOST_MD5:0:32}" ]; then
      echo -n "$ANDROID_DST "
      $ADB $DEVICE_SERIAL push $HOST_SRC $ANDROID_DST
    fi
  elif [ "${ANDROID_LS:0:1}" == "d" ]; then
    for FILE_ITEM in `ls $HOST_SRC`; do
      adb_push_if_needed "${HOST_SRC}/${FILE_ITEM}" "${ANDROID_DST}/${FILE_ITEM}"
    done
  else
    HOST_LS=`ls -ld $HOST_SRC`
    if [ "${HOST_LS:0:1}" == "d" ]; then
      $ADB $DEVICE_SERIAL shell mkdir -p $ANDROID_DST
      adb_push_if_needed $HOST_SRC $ANDROID_DST
    else
      echo -n "$ANDROID_DST "
      $ADB $DEVICE_SERIAL shell mkdir -p "$(dirname "$ANDROID_DST")"
      $ADB $DEVICE_SERIAL push $HOST_SRC $ANDROID_DST
    fi
  fi
}

setup_device "${DEVICE_ID}"
