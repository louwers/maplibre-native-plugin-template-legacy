#!/usr/bin/env bash
set -euo pipefail

# JitPack supplies VERSION and GROUP. Plugins compile against the plugin-enabled
# MapLibre Android SDK pinned in gradle.properties (Maven Central) and never
# bundle a renderer; applications choose the OpenGL or Vulkan SDK artifact.
: "${VERSION:?JitPack VERSION is required}"
: "${GROUP:?JitPack GROUP is required}"
cd "$(dirname "$0")/.."
./gradlew publishToMavenLocal -PpluginVersion="$VERSION" -PpluginGroup="$GROUP"
