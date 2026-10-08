plugins {
    id("com.android.application")
}

val maplibreVersion = providers.gradleProperty("maplibreVersion")

// Each plugin owns its demo activity, style assets and resources.
val pluginExamples = rootDir.resolve("plugins")
    .listFiles { plugin -> plugin.resolve("examples/android").isDirectory }
    .orEmpty()
    .sortedBy { it.name }
    .map { it.resolve("examples/android") }

android {
    namespace = "org.maplibre.plugins.demo"
    compileSdk = 35

    defaultConfig {
        applicationId = "org.maplibre.plugins.demo"
        minSdk = 23
        targetSdk = 35
        versionCode = 1
        versionName = "1.0"
        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
    }

    flavorDimensions += "renderer"
    productFlavors {
        create("opengl") { dimension = "renderer" }
        create("vulkan") { dimension = "renderer" }
    }

    sourceSets {
        getByName("main") {
            java.srcDirs(pluginExamples.map { it.resolve("java") })
            assets.srcDirs(pluginExamples.map { it.resolve("assets") })
            res.srcDirs(pluginExamples.map { it.resolve("res") })
        }
        getByName("androidTest") {
            java.srcDirs(pluginExamples.map { it.resolve("androidTest/java") })
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_11
        targetCompatibility = JavaVersion.VERSION_11
    }
}

dependencies {
    "openglImplementation"("org.maplibre.gl:android-sdk-opengl:${maplibreVersion.get()}")
    "vulkanImplementation"("org.maplibre.gl:android-sdk-vulkan:${maplibreVersion.get()}")
    implementation(project(":plugins:ngon-layer"))
    implementation(project(":plugins:rectangle-layer"))
    androidTestImplementation("androidx.test:core:1.7.0")
    androidTestImplementation("androidx.test:runner:1.7.0")
    androidTestImplementation("androidx.test:rules:1.7.0")
    androidTestImplementation("androidx.test.ext:junit:1.3.0")
}
