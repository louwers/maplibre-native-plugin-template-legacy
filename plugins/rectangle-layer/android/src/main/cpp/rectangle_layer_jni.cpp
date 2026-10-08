#include <jni.h>

#include <dlfcn.h>

#include "rectangle_layer.hpp"

namespace {

jobject makeResult(JNIEnv* env, mln_plugin_status status, const char* message) {
    jclass type = env->FindClass("org/maplibre/plugins/rectangle/RectangleLayerPlugin$NativeResult");
    if (!type) return nullptr;
    jmethodID constructor = env->GetMethodID(type, "<init>", "(ILjava/lang/String;)V");
    if (!constructor) return nullptr;
    jstring javaMessage = env->NewStringUTF(message ? message : "");
    jobject result = env->NewObject(type, constructor, static_cast<jint>(status), javaMessage);
    env->DeleteLocalRef(javaMessage);
    env->DeleteLocalRef(type);
    return result;
}

} // namespace

extern "C" JNIEXPORT jobject JNICALL
Java_org_maplibre_plugins_rectangle_RectangleLayerPlugin_nativeRegister(JNIEnv* env, jclass) {
    // Resolve the entry point from the renderer MapLibre already loaded; never load a second copy.
    // Multi-backend SDKs load the OpenGL renderer as libmaplibre-opengl.so, all others as libmaplibre.so.
    constexpr const char* libraries[] = {"libmaplibre-opengl.so", "libmaplibre.so"};
    void* host = nullptr;
    for (const char* library : libraries) {
        if ((host = dlopen(library, RTLD_NOW | RTLD_NOLOAD))) break;
    }
    if (!host) {
        return makeResult(env, MLN_PLUGIN_STATUS_NOT_FOUND, "Initialize MapLibre before registering plugins");
    }
    const auto registerPlugin = reinterpret_cast<mln_plugin_register_function_v1>(dlsym(host, "mln_plugin_register_v1"));
    if (!registerPlugin) {
        dlclose(host);
        return makeResult(env, MLN_PLUGIN_STATUS_NOT_FOUND, "This MapLibre SDK build does not enable the plugin API");
    }
    char error[512]{};
    const auto status = mln_rectangle_layer_register(registerPlugin, error, sizeof(error));
    dlclose(host); // MapLibre keeps its own reference for the process lifetime.
    return makeResult(env, status, error);
}
