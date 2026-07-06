#!/bin/bash

error=0

BD="$1"
if [[ -z "${BD}" ]] ; then
    echo "Usage: build-ios-luajit.sh \"BUILDDIR\""
    exit 1
fi
echo "Building iOS LuaJIT in \"${BD}\""

SD="${BD}/src"
OD="${BD}/bin"
HD="${BD}/include"

rm -rf "${BD}" || error=1
mkdir -p "${BD}" || error=1

mkdir "${SD}" || error=1
mkdir -p "${OD}/iphoneos" || error=1
mkdir -p "${OD}/iphonesimulator-arm64" || error=1
mkdir -p "${OD}/iphonesimulator-x86_64" || error=1
mkdir -p "${OD}/iphonesimulator" || error=1
mkdir "${HD}" || error=1

cp -r LuaJIT "${SD}" || error=1
cd "${SD}/LuaJIT" || error=1

export MACOSX_DEPLOYMENT_TARGET=10.9

ISDKP=$(xcrun --sdk iphoneos --show-sdk-path)
SSDKP=$(xcrun --sdk iphonesimulator --show-sdk-path)

# 1. Build for iOS Device (arm64)
make clean || error=1
make amalg -j HOST_CC="clang -DLUAJIT_DISABLE_JIT" TARGET_CC="clang -target arm64-apple-ios12.0 -isysroot ${ISDKP} -fvisibility=hidden -fvisibility-inlines-hidden" TARGET_CFLAGS="-O3 -DLUAJIT_DISABLE_JIT"
cp src/lib*a "${OD}/iphoneos/libluajit.a" || error=1

# 2. Build for iOS Simulator (arm64)
make clean || error=1
make amalg -j HOST_CC="clang -DLUAJIT_DISABLE_JIT" TARGET_CC="clang -target arm64-apple-ios12.0-simulator -isysroot ${SSDKP} -fvisibility=hidden -fvisibility-inlines-hidden" TARGET_CFLAGS="-O3 -DLUAJIT_DISABLE_JIT"
cp src/lib*a "${OD}/iphonesimulator-arm64/libluajit.a" || error=1

# 3. Build for iOS Simulator (x86_64)
make clean || error=1
make amalg -j HOST_CC="clang -DLUAJIT_DISABLE_JIT" TARGET_CC="clang -target x86_64-apple-ios12.0-simulator -isysroot ${SSDKP} -fvisibility=hidden -fvisibility-inlines-hidden" TARGET_CFLAGS="-O3 -DLUAJIT_DISABLE_JIT"
cp src/lib*a "${OD}/iphonesimulator-x86_64/libluajit.a" || error=1

# 4. Combine simulator libraries
lipo -create -arch arm64 "${OD}/iphonesimulator-arm64/libluajit.a" -arch x86_64 "${OD}/iphonesimulator-x86_64/libluajit.a" -output "${OD}/iphonesimulator/libluajit.a" || error=1

# 5. Create XCFramework
rm -rf "${OD}/libluajit.xcframework"
xcodebuild -create-xcframework \
  -library "${OD}/iphoneos/libluajit.a" \
  -library "${OD}/iphonesimulator/libluajit.a" \
  -output "${OD}/libluajit.xcframework" || error=1

# 6. Copy headers
cp src/*.h "${HD}" || error=1
cp src/*.hpp "${HD}" || error=1

if [ "$error" -ne 0 ] ; then
    exit 1
fi
exit 0
