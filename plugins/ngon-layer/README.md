# N-gon layer

Regular convex polygons centered on GeoJSON or vector-tile Point/MultiPoint features.
Register the plugin using the platform entry points below before loading the style.
The style layer type is `ngon`; the plugin ID is `org.maplibre.ngon-layer`.

```json
{
  "id": "markers",
  "type": "ngon",
  "source": "points",
  "paint": {
    "ngon-corners": ["get", "corners"],
    "ngon-radius": ["interpolate", ["linear"], ["zoom"], 10, 6, 16, 24],
    "ngon-rotate": ["get", "heading"],
    "ngon-color": ["get", "color"],
    "ngon-stroke-width": 2,
    "ngon-stroke-color": "#ffffff"
  }
}
```

Every property below supports constants, camera, feature, composite and feature-state
expressions. Numeric values and colors support transitions; enum strings do not.
All properties are paint properties. Ordinary source-layer, filter, visibility and
minzoom/maxzoom behavior comes from the host's source-bound layer implementation.

| Property | Default | Meaning |
| --- | --- | --- |
| `ngon-radius` | `5` | Circumradius in logical pixels; nonnegative. |
| `ngon-corners` | `5` | 3–360 corners, rounded to the nearest integer. |
| `ngon-rotate` | `0` | Clockwise degrees; zero places one vertex upward. |
| `ngon-color` | `#000000` | Fill color, including alpha. |
| `ngon-opacity` | `1` | Fill opacity, 0–1. |
| `ngon-blur` | `0` | Inward edge feather as a fraction of radius, 0–1. |
| `ngon-stroke-width` | `0` | Outside stroke width in logical pixels; mitered corners. |
| `ngon-stroke-color` | `#000000` | Stroke color, including alpha. |
| `ngon-stroke-opacity` | `1` | Stroke opacity, 0–1. |
| `ngon-translate` | `[0, 0]` | Pixel offset: right, down. |
| `ngon-translate-anchor` | `map` | `map` rotates the offset with the map; `viewport` does not. |
| `ngon-pitch-alignment` | `viewport` | `viewport` faces the camera; `map` lies in the map plane. |
| `ngon-pitch-scale` | `map` | `map` scales with perspective; `viewport` maintains size. |

Rotation is measured in the alignment plane. The map-aligned case rotates with the
map. As with circle markers, this is not a collision-placed symbol layer. Sort keys,
terrain/globe projection, patterns and collision placement are not implemented.
Transparent paint is still queryable, matching geometric hit testing; zero radius
and zero stroke produce no hit. Queries use the actual rotated polygon, not its quad.

## Implementation and validation

A four-vertex host-owned drawable carries each point. The shaders evaluate the
polygon analytically, with derivative antialiasing and premultiplied color blending.
Half-open tile ownership avoids duplicate buffered markers; marker quads are not
stencil-clipped at tile seams. Scalar and enum zoom endpoints are packed into float2
attributes so all thirteen properties can be feature-driven at once within the
portable sixteen-attribute limit. Geometry and GPU resources stay owned by MapLibre.

The same generator produces OpenGL ES, Vulkan and Metal shader sources. The
generated header is checked in so Swift Package Manager builds need no Node.js;
regenerate it after changing the generator:

```sh
node plugins/ngon-layer/scripts/generate-shaders.mjs \
  --output plugins/ngon-layer/shared/include/ngon_shader_sources.hpp
cmake --build build-Metal --target ngon-unit-tests
ctest --test-dir build-Metal -R '^ngon-unit-tests$' --output-on-failure
```

The shared render runner discovers this plugin's manifest automatically. Its cases
cover corner counts/rotation, every property feature-driven simultaneously, fractional
composite zoom, feature state, runtime paint updates, tile seams/MultiPoint, blur,
translation and all four pitch alignment/scale combinations. Expected images are
shared across backends; backend-specific skips or baselines are not needed.

## Android

Follow the [Android setup](../../README.md#android-setup) to configure JitPack
and the plugin-enabled MapLibre SDK. Add this plugin's dependency:

```kotlin
dependencies {
    implementation("org.maplibre.gl:android-sdk-opengl:13.6.1-pre935d410353da9d701ad42c97f2e0c300c8d408b8")
    implementation("com.github.louwers.maplibre-native-plugin-template:ngon-layer:<version-or-commit>")
}
```

For Vulkan, replace `android-sdk-opengl` with `android-sdk-vulkan`; do not add both.
Initialize MapLibre, then register before loading the style:

```kotlin
import org.maplibre.android.MapLibre
import org.maplibre.plugins.ngon.NgonLayerPlugin

MapLibre.getInstance(context)
NgonLayerPlugin.register()
```

Build the plugin from the repository root:

```sh
./gradlew :plugins:ngon-layer:assembleRelease
```

### Android example

Install the gallery and choose **Capital Atlas**:

```sh
./gradlew :examples:android-app:app:installOpenglDebug
adb shell am start -n org.maplibre.plugins.demo/.MainActivity
```

Use `installVulkanDebug` to run the Vulkan variant.

**Capital Atlas** is the Android n-gon demonstration. It downloads OpenFreeMap's
[Positron style](https://tiles.openfreemap.org/styles/positron) and reuses its live
vector `place` source. The [OpenMapTiles `capital` field](https://openmaptiles.org/schema/#place)
is an administrative level, not a boolean: level 2 is a gold hexagon, levels 3–4
are teal pentagons, and levels 5–6 are coral diamonds. Administrative terminology
varies by country. Colors, corners, rotation and zoom-interpolated radii are
feature-driven; the style expressions live in
[ngon-capitals.layers.json](examples/android/assets/ngon-capitals.layers.json).
The demo is map-only: pan, zoom and tilt with the standard map gestures, without
custom controls or overlays. More capitals become available as the provider's
tile zoom increases. An Internet connection is required; no capital coordinates
are hard-coded. The live device instrumentation test verifies both feature queries
and marker-colored pixels before/after a programmatic paint-property update:

```sh
./gradlew :examples:android-app:app:connectedVulkanDebugAndroidTest \
  -Pandroid.testInstrumentationRunnerArguments.class=org.maplibre.plugins.demo.CapitalExplorerTest
```

## iOS / Metal

Follow the [iOS setup](../../README.md#ios-setup): add the plugin-enabled
`MapLibre` product from `louwers/maplibre-ios-with-plugin-api` and this
repository's `NgonLayer` product. Register before loading a style that uses
this plugin:

```swift
import NgonLayer

try NgonLayerPlugin.registerPlugin()
```

Build the plugin for the simulator from the repository root:

```sh
xcodebuild -scheme NgonLayer -destination 'generic/platform=iOS Simulator' build
```

### iOS example

Open `examples/ios-app/PluginGallery.xcodeproj` and run the `PluginGallery` scheme
with `PLUGIN_SCENE=ngon` in the scheme environment, or run the gallery UI test,
which screenshots every scene:

```sh
examples/ios-app/scripts/test.sh SIMULATOR_UDID
```

The iOS scene uses the synthetic point-marker style; Capital Atlas is currently
an Android demonstration.

## Running render tests

Use the [shared runner instructions](../../render-tests/README.md). To run only
this plugin after building the Metal runner:

```sh
build-Metal/plugin-render-tests \
  --manifestPath plugins/ngon-layer/render-tests/manifest.json --recycle-map
```
