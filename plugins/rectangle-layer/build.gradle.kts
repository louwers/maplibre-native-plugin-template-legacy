plugins {
    id("com.android.library")
    id("maven-publish")
}

val pluginVersion = providers.gradleProperty("pluginVersion")
val pluginGroup = providers.gradleProperty("pluginGroup").orElse("org.maplibre.plugins")
val pluginAbis = providers.gradleProperty("maplibrePluginAbis").orNull
val maplibreVersion = providers.gradleProperty("maplibreVersion")

group = pluginGroup.get()
version = pluginVersion.get()

android {
    namespace = "org.maplibre.plugins.rectangle"
    compileSdk = 35
    ndkVersion = "28.2.13676358"

    defaultConfig {
        minSdk = 23
        if (pluginAbis != null) {
            ndk { abiFilters += pluginAbis.split(',').map(String::trim).filter { it.isNotEmpty() } }
        }
        externalNativeBuild {
            cmake {
                cppFlags += listOf("-std=c++20", "-fvisibility=hidden")
                arguments += listOf(
                    "-DANDROID_STL=c++_static",
                    "-DMLN_RECTANGLE_PLUGIN_VERSION=${pluginVersion.get()}"
                )
            }
        }
    }

    externalNativeBuild {
        cmake {
            path = file("android/src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }
    buildFeatures { prefab = true }
    sourceSets {
        getByName("main") {
            manifest.srcFile("android/src/main/AndroidManifest.xml")
            java.setSrcDirs(listOf("android/src/main/java"))
        }
    }
    packaging.jniLibs.excludes += setOf("**/libc++_shared.so", "**/libmaplibre.so", "**/libmaplibre-opengl.so")
    publishing { singleVariant("release") { withSourcesJar() } }
}

dependencies {
    // Compile against a plugin-enabled MapLibre SDK: its Java style types and the
    // header-only Prefab module that provides <mln/plugin/plugin_api.h>. The
    // application selects the renderer artifact (OpenGL or Vulkan) at runtime.
    compileOnly("org.maplibre.gl:android-sdk-opengl:${maplibreVersion.get()}")
}

publishing {
    publications {
        register<MavenPublication>("release") {
            groupId = project.group.toString()
            artifactId = "rectangle-layer"
            version = project.version.toString()
            afterEvaluate { from(components["release"]) }
            pom {
                name.set("MapLibre Rectangle Layer")
                description.set("Source-bound point rectangle style layer for MapLibre Native")
                url.set("https://github.com/louwers/maplibre-native-plugin-template")
                licenses {
                    license {
                        name.set("BSD-2-Clause")
                        url.set("https://opensource.org/license/bsd-2-clause")
                    }
                }
            }
        }
    }
}
