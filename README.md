# MapLibre Native plugin template

Infrastructure for independent, source-bound MapLibre Native layer plugins on
Android (OpenGL and Vulkan) and iOS (Metal). Plugins register new layer types
through MapLibre Native's C plugin API; they do not patch the core or add
properties to built-in layers.

Each directory under [plugins/](plugins/) owns its implementation and README.
Start with that plugin's README for its style schema, registration API and
examples. This template currently ships two plugins:

- [`ngon-layer`](plugins/ngon-layer/): regular polygons on point features.
- [`rectangle-layer`](plugins/rectangle-layer/): screen-aligned rectangles on point features.

## Repository structure

Plugins separate shared C++ layout, properties, and shader sources from Android
JNI/Java and iOS Objective-C wrappers. The host owns all GPU resources.

- `plugins/<plugin>/shared`: cross-platform implementation.
- `plugins/<plugin>/android` and `ios`: platform wrappers.
- `plugins/<plugin>/render-tests`: fixtures and reviewed expected images.
- `examples/`: platform sample applications.

Run `node scripts/generate.mjs` after adding a plugin or changing its
`plugin.json` or `swift-targets.swift`; it regenerates `Package.swift`, the
example catalogs, and the release workflow.

## Host SDKs

The plugin API is not part of ordinary MapLibre releases. The template builds
against plugin-enabled prereleases that expose the same `plugin_api.h`:

| Platform | Host SDK | Pinned in |
| --- | --- | --- |
| Android | `org.maplibre.gl:android-sdk-opengl` / `android-sdk-vulkan` `13.6.1-pre935d410353da9d701ad42c97f2e0c300c8d408b8` (Maven Central) | `gradle.properties` (`maplibreVersion`) |
| iOS | [`louwers/maplibre-ios-with-plugin-api`](https://github.com/louwers/maplibre-ios-with-plugin-api) `7.0.0-pre1` (Swift Package Manager) | `maplibre-ios-version.txt` |
| Render tests | MapLibre Native source at the iOS release commit | `native-revision.txt` |

Plugins only compile against the C header and resolve `mln_plugin_register_v1`
from the host at runtime, so they never bundle or select a renderer. Always
register plugins before loading a style that uses their layer types.

## Android setup

Applications choose exactly one renderer artifact and add the plugin artifacts:

```kotlin
dependencies {
    implementation("org.maplibre.gl:android-sdk-opengl:13.6.1-pre935d410353da9d701ad42c97f2e0c300c8d408b8")
    implementation("com.github.louwers.maplibre-native-plugin-template:ngon-layer:<version-or-commit>")
}
```

Use `android-sdk-vulkan` instead of `android-sdk-opengl` for Vulkan; do not add
both. Plugins are published through JitPack:

```kotlin
dependencyResolutionManagement {
    repositories {
        google()
        mavenCentral()
        maven("https://jitpack.io") {
            content { includeGroup("com.github.louwers.maplibre-native-plugin-template") }
        }
    }
}
```

Each plugin module declares the SDK as `compileOnly`. Its Prefab module
`MapLibreAndroid::maplibre` supplies `<mln/plugin/plugin_api.h>` to CMake, and the
Java wrapper uses the SDK's style types. At registration the JNI code finds
`mln_plugin_register_v1` in the already loaded renderer (`libmaplibre.so`, or
`libmaplibre-opengl.so` in multi-backend builds) with `dlopen(RTLD_NOLOAD)` and
`dlsym`.

Build the example gallery (configure the Android SDK in `local.properties` or
`ANDROID_HOME`):

```sh
./gradlew :examples:android-app:app:installOpenglDebug
adb shell am start -n org.maplibre.plugins.demo/.MainActivity
```

Use `installVulkanDebug` for Vulkan, and `-PmaplibrePluginAbis=arm64-v8a` to
limit the plugin ABIs while iterating. To test a locally published SDK, pass
`-PmaplibreVersion=<version>` and, if it is not on Maven Central,
`-PmaplibreRepositoryUrl=<repository-url>` (for example `file://$HOME/.m2/repository`).

## iOS setup

Add two Swift packages to the application:

- `https://github.com/louwers/maplibre-ios-with-plugin-api`, exact version
  `7.0.0-pre1`, product **`MapLibre`**. It is a drop-in replacement for the
  standard MapLibre iOS package: keep `import MapLibre` / `#import <MapLibre/MapLibre.h>`.
  Do not add the standard `maplibre-gl-native-distribution` package as well.
- `https://github.com/louwers/maplibre-native-plugin-template`, and select the
  plugin products you need (for example `NgonLayer`).

Plugin targets depend only on the header-only `MapLibrePluginApi` product of the
first package, which provides `<mln/plugin/plugin_api.h>`; the application links
the `MapLibre` framework that exports `mln_plugin_register_v1`. Because both come
from the same package version, SwiftPM keeps the plugin API and the SDK in sync.

```swift
import MapLibre
import NgonLayer

try NgonLayerPlugin.registerPlugin()
```

### iOS example gallery

[`examples/ios-app`](examples/ios-app/) is an Xcode project that consumes both
packages. Open `examples/ios-app/PluginGallery.xcodeproj` and run the
`PluginGallery` scheme, or run its UI test, which screenshots every plugin scene:

```sh
xcrun simctl list devices available
examples/ios-app/scripts/test.sh SIMULATOR_UDID
```

Set `PLUGIN_SCENE=ngon` or `PLUGIN_SCENE=rectangle` in the scheme's environment
to choose the initial scene. The project is generated; after adding a plugin,
run `ruby examples/ios-app/scripts/generate-project.rb` (requires the
`xcodeproj` gem).

### Testing an unreleased MapLibre iOS build

Set `MAPLIBRE_IOS_PATH` to a local checkout of the
`maplibre-ios-with-plugin-api` package (for example one whose binary target
points at a locally built XCFramework). The template's `Package.swift` and the
project generator both honor it:

```sh
export MAPLIBRE_IOS_PATH=/path/to/maplibre-ios-with-plugin-api
xcodebuild -scheme NgonLayer -destination 'generic/platform=iOS Simulator' build
ruby examples/ios-app/scripts/generate-project.rb
examples/ios-app/scripts/test.sh SIMULATOR_UDID
```

Regenerate the project without the variable before committing it.

## Render tests

Render tests link MapLibre Native from source, because the harness is not part of
the SDKs. CI checks out `native-revision.txt` and builds the shared CMake runner
with `MLN_WITH_PLUGINS=ON` for Linux OpenGL, Linux Vulkan and macOS Metal. See
[render-tests/README.md](render-tests/README.md).

## Publishing

`.github/workflows/release-plugin.yml` builds a plugin's Android AAR against the
pinned SDK and its iOS XCFramework, then attaches both (with SHA-256 checksums) to a
GitHub release. Set its `publish` input to false to only build them as workflow
artifacts. JitPack builds the Android modules with `scripts/jitpack-build.sh`.

`scripts/build-ios-xcframework.sh PLUGIN VERSION OUTPUT_DIR` builds the iOS
XCFramework locally. It is a static framework containing only the plugin, built for
iOS devices and simulators from the plugin's Swift package target. Applications link
it next to the `MapLibre` product of `maplibre-ios-with-plugin-api`, which provides
`mln_plugin_register_v1`, and import it with `#import <NgonLayer/NgonLayer.h>`. Its
module map autolinks libc++ and Foundation. iOS plugins are also available as source
through this repository's Swift package at the same tag.

## Plugin API compared to the previous template revision

Earlier revisions of this template targeted an experimental, larger plugin API
(MapLibre Native `e22b451`, the closed
[maplibre-native#4610](https://github.com/maplibre/maplibre-native/pull/4610)).
The released SDKs ship the smaller v1 API from MapLibre Native `main`. The heatmap,
hillshade and glTF plugins depended on removed features and were deleted.

Removed from the API:

- Render graphs: `mln_plugin_render_graph_v1`, render targets, render passes,
  texture bindings and shader textures (`mln_plugin_shader_texture_v1`,
  `textures`/`texture_count` in shader descriptors), viewport quads and tile
  projections.
- Non-geometry sources: `mln_plugin_source_kind` and raster/raster-DEM layers,
  including DEM fields in the uniform context.
- Host services: `mln_plugin_host_api_v1` (logging, resource requests, repaint
  requests) and `host` in the layout context.
- Layout-scope properties (`mln_plugin_property_scope`); all properties are paint
  properties.
- Boolean, float-array, color-array and color-ramp values, and with them
  `accepts_scalar`, `maximum_array_length` and `MLN_PLUGIN_PROPERTY_ENCODING_BOOLEAN_FLOAT`.
- Per-drawable draw mode, render stage and blend mode, per-attribute types in
  attribute bindings, and `feature_index` on segments. Drawables are indexed
  triangles in the translucent pass with premultiplied-alpha blending.
- Feature IDs, `properties_json` and evaluated properties on features; tile
  coordinates, layer IDs and serialized properties in the layout context;
  pass/tile/zoom/render-target fields in the uniform context.
- Layer-type `render_stage`, `requires_3d`, `participates_in_3d_pass` and
  `render_graph`; and the `mln_plugin_is_registered_v1`, `mln_plugin_count_v1`
  and `mln_plugin_id_at_v1` queries.

Renamed or added:

- Uniform scopes are `MLN_PLUGIN_UNIFORM_DRAWABLE`, `MLN_PLUGIN_UNIFORM_LAYER`
  and `MLN_PLUGIN_UNIFORM_DRAWABLE_ARRAY` (was `..._SCOPE_LAYER`/`..._SCOPE_DRAWABLE`).
- Drawables select depth with `MLN_PLUGIN_DRAWABLE_DEPTH_*` and may opt into
  `enable_stencil_overlap` and `cull_back_faces`; layer types may set
  `enable_stencil_overlap_dedup`, `enable_near_clipped_matrix` and a
  `should_animate` callback.
- Value type enum values were renumbered (`MLN_PLUGIN_VALUE_FLOAT` is now `1`),
  so plugins must be recompiled against the new header.

Porting a geometry plugin means deleting the removed fields from its designated
initializers; see the ngon and rectangle changes in this repository's history.
