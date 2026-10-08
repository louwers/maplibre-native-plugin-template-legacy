#!/bin/bash
# Builds the gallery and runs its UI test, which screenshots every plugin scene.
set -euo pipefail
cd "$(dirname "$0")/.."
if [[ $# != 1 ]]; then
    echo "Usage: $0 SIMULATOR_UDID (see: xcrun simctl list devices available)" >&2
    exit 2
fi
mkdir -p build
result="build/PluginGallery-$(date +%Y%m%d-%H%M%S).xcresult"
xcodebuild test -project PluginGallery.xcodeproj -scheme PluginGallery \
    -destination "platform=iOS Simulator,id=$1" \
    -derivedDataPath build/DerivedData -resultBundlePath "$result" \
    CODE_SIGNING_ALLOWED=NO
echo "Scene screenshots: $result"
