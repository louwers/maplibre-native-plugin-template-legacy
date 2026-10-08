# Rectangle layer

The rectangle plugin registers a source-bound `rectangle` style layer. It consumes Point and MultiPoint features from GeoJSON or vector-tile geometry sources. Every point becomes a centered, viewport-aligned rectangle measured in logical screen pixels.

```json
{
  "id": "points",
  "type": "rectangle",
  "source": "points",
  "paint": {
    "rectangle-color": "#e00",
    "rectangle-width": 12,
    "rectangle-height": 12,
    "rectangle-stroke-width": 1,
    "rectangle-stroke-color": "#000"
  }
}
```

All five paint properties accept MapLibre expressions. The host evaluates them per feature during tile layout, copies the returned CPU bucket, compiles the plugin-provided OpenGL/Vulkan/Metal shader, and owns every resulting GPU buffer and drawable. Register the plugin before loading a style containing the layer.

## Android

Follow the [Android setup](../../README.md#android-setup) to configure JitPack
and the plugin-enabled MapLibre SDK. Add this plugin's dependency:

```kotlin
dependencies {
    implementation("org.maplibre.gl:android-sdk-opengl:13.6.1-pre935d410353da9d701ad42c97f2e0c300c8d408b8")
    implementation("com.github.louwers.maplibre-native-plugin-template:rectangle-layer:<version-or-commit>")
}
```

For Vulkan, replace `android-sdk-opengl` with `android-sdk-vulkan`; do not add both.
Initialize MapLibre, then register before loading the style:

```kotlin
import org.maplibre.android.MapLibre
import org.maplibre.plugins.rectangle.RectangleLayerPlugin

MapLibre.getInstance(context)
RectangleLayerPlugin.register()
```

Build the plugin from the repository root:

```sh
./gradlew :plugins:rectangle-layer:assembleRelease
```

### Android example

Install the gallery and choose **Rectangle layer**:

```sh
./gradlew :examples:android-app:app:installOpenglDebug
adb shell am start -n org.maplibre.plugins.demo/.MainActivity
```

Use `installVulkanDebug` to run the Vulkan variant.

The Android sample demonstrates feature-driven paint and smoothly interpolated
zoom styling, with a button to animate between zoom levels 10 and 16.

## iOS / Metal

Follow the [iOS setup](../../README.md#ios-setup): add the plugin-enabled
`MapLibre` product from `louwers/maplibre-ios-with-plugin-api` and this
repository's `RectangleLayer` product. Register before loading a style that uses
this plugin:

```swift
import RectangleLayer

try RectangleLayerPlugin.registerPlugin()
```

Build the plugin for the simulator from the repository root:

```sh
xcodebuild -scheme RectangleLayer -destination 'generic/platform=iOS Simulator' build
```

### iOS example

Open `examples/ios-app/PluginGallery.xcodeproj` and run the `PluginGallery` scheme
with `PLUGIN_SCENE=rectangle` in the scheme environment, or run the gallery UI test,
which screenshots every scene:

```sh
examples/ios-app/scripts/test.sh SIMULATOR_UDID
```

## Running render tests

Use the [shared runner instructions](../../render-tests/README.md). To run only
this plugin after building the Metal runner:

```sh
build-Metal/plugin-render-tests \
  --manifestPath plugins/rectangle-layer/render-tests/manifest.json --recycle-map
```
