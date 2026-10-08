.target(
            name: "RectangleLayer",
            dependencies: [.product(name: "MapLibrePluginApi", package: mapLibreIOSPackage)],
            path: "plugins/rectangle-layer",
            exclude: [
                "README.md",
                "plugin.json", "swift-targets.swift", "examples",
                "android",
                "render-tests",
            ],
            sources: [
                "ios/src/RectangleLayer.mm",
                "shared/cpp/rectangle_layer.cpp",
            ],
            publicHeadersPath: "ios/include",
            cxxSettings: [
                .headerSearchPath("shared/include"),
                .define("MLN_RECTANGLE_PLUGIN_VERSION", to: "\"\(pluginVersion)\""),
            ],
            linkerSettings: [
                .linkedFramework("Foundation"),
            ]
        ),
