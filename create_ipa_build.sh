#!/bin/bash

# Ensure Payload directory exists and is clean
rm -rf build_ios/Payload
mkdir -p build_ios/Payload

cmake -Bbuild_ios -GXcode -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_ARCHITECTURES=arm64 -DSURGE_SKIP_DISTRIBUTION=TRUE
xcodebuild -project build_ios/Surge.xcodeproj -scheme surge-xt_Standalone -configuration Release -sdk iphoneos -allowProvisioningUpdates build

cp -R "build_ios/src/surge-xt/surge-xt_artefacts/Release/Standalone/Surge XT.app" build_ios/Payload/                                          
cd build_ios && zip -qr "Surge XT.ipa" Payload && cd ..                                             
echo "Done. IPA is in ./build_ios/Surge XT.ipa"