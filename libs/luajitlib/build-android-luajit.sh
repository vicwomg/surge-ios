#!/bin/bash

# This script builds LuaJIT for Android using the NDK.

error=0

BD="$1"
ANDROID_NDK="$2"
ANDROID_ABI="$3"
ANDROID_PLATFORM="$4"
CMAKE_C_COMPILER="$5"
CMAKE_AR="$6"
CMAKE_RANLIB="$7"
CMAKE_STRIP="$8"
CMAKE_C_FLAGS="$9"

if [[ -z "${BD}" ]] ; then
    echo "Usage: build-android-luajit.sh \"BUILDDIR\" \"NDK\" \"ABI\" \"PLATFORM\" \"CC\" \"AR\" \"RANLIB\" \"STRIP\" \"CFLAGS\""
    exit 1
fi

SD="${BD}/src"
OD="${BD}/bin"
HD="${BD}/include"

rm -rf "${BD}" || error=1
mkdir -p "${BD}" || error=1

mkdir "${SD}" || error=1
mkdir "${OD}" || error=1
mkdir "${HD}" || error=1

cp -r LuaJIT "${SD}" || error=1
cd "${SD}/LuaJIT/src" || error=1

# Determine host architecture for building buildvm (minilua)
HOST_OS=$(uname -s | tr '[:upper:]' '[:lower:]')
HOST_ARCH=$(uname -m)
HOST_CC="cc"

make clean

# Extract API level from platform (e.g., android-26 -> 26)
API_LEVEL=$(echo ${ANDROID_PLATFORM} | sed 's/android-//')

# Using -target for clang to ensure it knows it's targeting Linux/Android and uses ELF
if [ "${ANDROID_ABI}" == "arm64-v8a" ]; then
    TARGET_FLAGS="--target=aarch64-none-linux-android${API_LEVEL}"
elif [ "${ANDROID_ABI}" == "x86_64" ]; then
    TARGET_FLAGS="--target=x86_64-none-linux-android${API_LEVEL}"
else
    TARGET_FLAGS=""
fi

# Build LuaJIT
# Explicitly set TARGET_SYS=Linux to avoid LuaJIT's Darwin-specific logic on macOS host
# Pass -D__ANDROID__ and -D__ANDROID_API__
make -j amalg \
    HOST_CC="${HOST_CC}" \
    STATIC_CC="${CMAKE_C_COMPILER} ${TARGET_FLAGS}" \
    DYNAMIC_CC="${CMAKE_C_COMPILER} ${TARGET_FLAGS} -fPIC" \
    TARGET_LD="${CMAKE_C_COMPILER} ${TARGET_FLAGS}" \
    TARGET_AR="${CMAKE_AR} rcus" \
    TARGET_STRIP="${CMAKE_STRIP}" \
    TARGET_SYS=Linux \
    TARGET_CFLAGS="-fPIC -fvisibility=hidden -D__ANDROID__ -D__ANDROID_API__=${API_LEVEL} ${CMAKE_C_FLAGS}" \
    || error=1

if [ "$error" -ne 0 ] ; then
    echo "LuaJIT build failed!"
    exit 1
fi

cp lib*a "${OD}" || error=1
cp *.h "${HD}" || error=1
cp *.hpp "${HD}" || error=1

exit 0
