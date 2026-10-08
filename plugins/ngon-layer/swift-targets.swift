.target(
            name: "NgonLayer",
            dependencies: [.product(name: "MapLibrePluginApi", package: mapLibreIOSPackage)],
            path: "plugins/ngon-layer",
            exclude: [
                "README.md",
                "plugin.json", "swift-targets.swift", "examples",
                "android",
                "render-tests", "scripts", "tests", "release.json", "build.gradle.kts",
            ],
            sources: [
                "ios/src/NgonLayer.mm",
                "shared/cpp/ngon_layer.cpp",
            ],
            publicHeadersPath: "ios/include",
            cxxSettings: [
                .headerSearchPath("shared/include"),
                .define("MLN_NGON_PLUGIN_VERSION", to: "\"\(pluginVersion)\""),
            ],
            linkerSettings: [
                .linkedFramework("Foundation"),
            ]
        ),
