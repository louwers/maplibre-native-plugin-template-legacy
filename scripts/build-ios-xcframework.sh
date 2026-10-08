#!/bin/bash
# Builds a plugin's SwiftPM target as a static XCFramework for iOS devices and simulators.
# Usage: scripts/build-ios-xcframework.sh PLUGIN VERSION OUTPUT_DIR
#
# The framework contains only the plugin. It leaves mln_plugin_register_v1 undefined;
# applications resolve it by linking the plugin-enabled MapLibre framework.
set -euo pipefail

if [[ $# != 3 ]]; then
    echo "Usage: $0 PLUGIN VERSION OUTPUT_DIR" >&2
    exit 2
fi
root="$(cd "$(dirname "$0")/.." && pwd)"
plugin="$1"
version="$2"
mkdir -p "$3"
output="$(cd "$3" && pwd)"

metadata="$root/plugins/$plugin/plugin.json"
product="$(jq -er .apple.product "$metadata")"
header="$(jq -er .apple.header "$metadata")"
registration_class="$(jq -er .apple.registrationClass "$metadata")"
headers="$root/plugins/$plugin/ios/include/$(dirname "$header")"
minimum_os="$(sed -n 's/.*platforms: \[\.iOS("\([0-9.]*\)")\].*/\1/p' "$root/Package.swift")"

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

frameworks=()
for platform in iphoneos iphonesimulator; do
    if [[ $platform == iphoneos ]]; then
        destination="generic/platform=iOS"
        archs="arm64"
    else
        destination="generic/platform=iOS Simulator"
        archs="arm64 x86_64"
    fi

    # Package.swift reads MLN_PLUGIN_VERSION for the plugin's version define.
    (cd "$root" && MLN_PLUGIN_VERSION="$version" xcodebuild build \
        -quiet \
        -scheme "$product" \
        -destination "$destination" \
        -configuration Release \
        -derivedDataPath "$work/DerivedData" \
        ARCHS="$archs" \
        ONLY_ACTIVE_ARCH=NO \
        CODE_SIGNING_ALLOWED=NO)

    object="$work/DerivedData/Build/Products/Release-$platform/$product.o"
    framework="$work/$platform/$product.framework"
    mkdir -p "$framework/Headers" "$framework/Modules"
    libtool -static -o "$framework/$product" "$object"
    cp "$headers"/*.h "$framework/Headers/"
    # Autolink the plugin's runtime dependencies for applications that import the module.
    cat > "$framework/Modules/module.modulemap" <<EOF
framework module $product {
    umbrella header "$(basename "$header")"
    export *
    module * { export * }
    link "c++"
    link framework "Foundation"
}
EOF
    plutil -create xml1 "$framework/Info.plist"
    plutil -insert CFBundleExecutable -string "$product" "$framework/Info.plist"
    plutil -insert CFBundleIdentifier -string "org.maplibre.plugins.$plugin" "$framework/Info.plist"
    plutil -insert CFBundleName -string "$product" "$framework/Info.plist"
    plutil -insert CFBundlePackageType -string FMWK "$framework/Info.plist"
    plutil -insert CFBundleShortVersionString -string "${version%%-*}" "$framework/Info.plist"
    plutil -insert CFBundleVersion -string "$version" "$framework/Info.plist"
    plutil -insert MinimumOSVersion -string "$minimum_os" "$framework/Info.plist"

    for arch in $archs; do
        symbols="$(nm -arch "$arch" "$framework/$product")"
        grep -q " [ST] _OBJC_CLASS_\$_$registration_class$" <<< "$symbols" ||
            { echo "$platform $arch: missing $registration_class" >&2; exit 1; }
        grep -q " U _mln_plugin_register_v1$" <<< "$symbols" ||
            { echo "$platform $arch: expected an undefined mln_plugin_register_v1" >&2; exit 1; }
        if grep -q " [TD] _mln_plugin_register_v1$" <<< "$symbols"; then
            echo "$platform $arch: the plugin must not contain the MapLibre host" >&2
            exit 1
        fi
    done
    frameworks+=(-framework "$framework")
done

xcodebuild -create-xcframework "${frameworks[@]}" -output "$work/$product.xcframework" > /dev/null
archive="$output/$product-$version.xcframework.zip"
rm -f "$archive"
(cd "$work" && ditto -c -k --norsrc --noextattr --keepParent "$product.xcframework" "$archive")
echo "$archive"
