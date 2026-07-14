#!/bin/bash
set -euo pipefail

# LuaJIT requires this when compiling host-side tools on macOS.
export MACOSX_DEPLOYMENT_TARGET="${MACOSX_DEPLOYMENT_TARGET:-12.0}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "Building Android APK using the Android Studio project..."
cd "${SCRIPT_DIR}/android"
./gradlew :app:assembleRelease

echo "Done. APK output is under android/app/build/outputs/apk/release/."
