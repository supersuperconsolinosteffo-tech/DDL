// entry_point.cpp
// Native library entry point and JNI exports

#include "native_client.hpp"
#include "jni/jni_bridge.hpp"
#include "imgui_manager.hpp"
#include "utilitymod_export.h"

#include <jni.h>
#include <mutex>

namespace utilitymod {

// Global native client instance
static std::once_flag initFlag;
static bool libraryLoaded = false;

// JNI native method implementations
extern "C" {

// Forward declarations (registered in JNI_OnLoad before their definitions)
JNIEXPORT void JNICALL Java_com_utilitymod_nativeclient_NativeBridge_nativeLog(JNIEnv*, jclass, jstring);
JNIEXPORT void JNICALL Java_com_utilitymod_nativeclient_NativeBridge_nativeError(JNIEnv*, jclass, jstring);
JNIEXPORT void JNICALL Java_com_utilitymod_nativeclient_NativeBridge_requestTickAction(JNIEnv*, jclass, jint, jint, jobject);
JNIEXPORT jdouble JNICALL Java_com_utilitymod_nativeclient_NativeBridge_getPlayerX(JNIEnv*, jclass);
JNIEXPORT jdouble JNICALL Java_com_utilitymod_nativeclient_NativeBridge_getPlayerY(JNIEnv*, jclass);
JNIEXPORT jdouble JNICALL Java_com_utilitymod_nativeclient_NativeBridge_getPlayerZ(JNIEnv*, jclass);
JNIEXPORT jfloat JNICALL Java_com_utilitymod_nativeclient_NativeBridge_getPlayerYaw(JNIEnv*, jclass);
JNIEXPORT jfloat JNICALL Java_com_utilitymod_nativeclient_NativeBridge_getPlayerPitch(JNIEnv*, jclass);
JNIEXPORT jboolean JNICALL Java_com_utilitymod_nativeclient_NativeBridge_isPlayerAlive(JNIEnv*, jclass);
JNIEXPORT jint JNICALL Java_com_utilitymod_nativeclient_NativeBridge_getEntityId(JNIEnv*, jclass);

// Called when the native library is loaded by Java
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    // Store the JavaVM for later use
    NativeClient::instance().getJniBridge()->initialize("");
    
    // Register native methods for NativeBridge
    JNIEnv* env;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_8) != JNI_OK) {
        return JNI_ERR;
    }

    // Find the NativeBridge class
    jclass bridgeClass = env->FindClass("com/utilitymod/nativeclient/NativeBridge");
    if (!bridgeClass) {
        return JNI_ERR;
    }

    // Native methods to register
    static const JNINativeMethod methods[] = {
        {"nativeLog", "(Ljava/lang/String;)V", reinterpret_cast<void*>(&Java_com_utilitymod_nativeclient_NativeBridge_nativeLog)},
        {"nativeError", "(Ljava/lang/String;)V", reinterpret_cast<void*>(&Java_com_utilitymod_nativeclient_NativeBridge_nativeError)},
        {"requestTickAction", "(IILjava/util/Map;)V", reinterpret_cast<void*>(&Java_com_utilitymod_nativeclient_NativeBridge_requestTickAction)},
        {"getPlayerX", "()D", reinterpret_cast<void*>(&Java_com_utilitymod_nativeclient_NativeBridge_getPlayerX)},
        {"getPlayerY", "()D", reinterpret_cast<void*>(&Java_com_utilitymod_nativeclient_NativeBridge_getPlayerY)},
        {"getPlayerZ", "()D", reinterpret_cast<void*>(&Java_com_utilitymod_nativeclient_NativeBridge_getPlayerZ)},
        {"getPlayerYaw", "()F", reinterpret_cast<void*>(&Java_com_utilitymod_nativeclient_NativeBridge_getPlayerYaw)},
        {"getPlayerPitch", "()F", reinterpret_cast<void*>(&Java_com_utilitymod_nativeclient_NativeBridge_getPlayerPitch)},
        {"isPlayerAlive", "()Z", reinterpret_cast<void*>(&Java_com_utilitymod_nativeclient_NativeBridge_isPlayerAlive)},
        {"getEntityId", "()I", reinterpret_cast<void*>(&Java_com_utilitymod_nativeclient_NativeBridge_getEntityId)},
    };

    if (env->RegisterNatives(bridgeClass, methods, sizeof(methods) / sizeof(methods[0])) != JNI_OK) {
        return JNI_ERR;
    }

    libraryLoaded = true;
    return JNI_VERSION_1_8;
}

// Called when the native library is unloaded
JNIEXPORT void JNICALL JNI_OnUnload(JavaVM* vm, void* reserved) {
    NativeClient::instance().shutdown();
    libraryLoaded = false;
}

// Native method implementations
JNIEXPORT void JNICALL Java_com_utilitymod_nativeclient_NativeBridge_nativeLog(JNIEnv* env, jclass clazz, jstring message) {
    const char* str = env->GetStringUTFChars(message, nullptr);
    if (str) {
        NativeClient::instance().log(str);
        env->ReleaseStringUTFChars(message, str);
    }
}

JNIEXPORT void JNICALL Java_com_utilitymod_nativeclient_NativeBridge_nativeError(JNIEnv* env, jclass clazz, jstring message) {
    const char* str = env->GetStringUTFChars(message, nullptr);
    if (str) {
        NativeClient::instance().error(str);
        env->ReleaseStringUTFChars(message, str);
    }
}

JNIEXPORT void JNICALL Java_com_utilitymod_nativeclient_NativeBridge_requestTickAction(JNIEnv* env, jclass clazz, jint moduleId, jint actionId, jobject paramsMap) {
    // Convert Java Map to C++ unordered_map
    std::unordered_map<std::string, std::string> params;
    
    if (paramsMap) {
        jclass mapClass = env->GetObjectClass(paramsMap);
        jmethodID entrySetMethod = env->GetMethodID(mapClass, "entrySet", "()Ljava/util/Set;");
        jobject entrySet = env->CallObjectMethod(paramsMap, entrySetMethod);
        
        jclass setClass = env->GetObjectClass(entrySet);
        jmethodID iteratorMethod = env->GetMethodID(setClass, "iterator", "()Ljava/util/Iterator;");
        jobject iterator = env->CallObjectMethod(entrySet, iteratorMethod);
        
        jclass iteratorClass = env->GetObjectClass(iterator);
        jmethodID hasNextMethod = env->GetMethodID(iteratorClass, "hasNext", "()Z");
        jmethodID nextMethod = env->GetMethodID(iteratorClass, "next", "()Ljava/lang/Object;");
        
        jclass entryClass = env->FindClass("java/util/Map$Entry");
        jmethodID getKeyMethod = env->GetMethodID(entryClass, "getKey", "()Ljava/lang/Object;");
        jmethodID getValueMethod = env->GetMethodID(entryClass, "getValue", "()Ljava/lang/Object;");
        
        while (env->CallBooleanMethod(iterator, hasNextMethod)) {
            jobject entry = env->CallObjectMethod(iterator, nextMethod);
            jobject key = env->CallObjectMethod(entry, getKeyMethod);
            jobject value = env->CallObjectMethod(entry, getValueMethod);
            
            if (key && value) {
                const char* keyStr = env->GetStringUTFChars(static_cast<jstring>(key), nullptr);
                const char* valueStr = env->GetStringUTFChars(static_cast<jstring>(value), nullptr);
                if (keyStr && valueStr) {
                    params[keyStr] = valueStr;
                }
                if (keyStr) env->ReleaseStringUTFChars(static_cast<jstring>(key), keyStr);
                if (valueStr) env->ReleaseStringUTFChars(static_cast<jstring>(value), valueStr);
            }
        }
    }

    NativeClient::instance().requestTickAction(
        static_cast<ModuleId>(moduleId),
        static_cast<ActionId>(actionId),
        params
    );
}

JNIEXPORT jdouble JNICALL Java_com_utilitymod_nativeclient_NativeBridge_getPlayerX(JNIEnv* env, jclass clazz) {
    // This would be called from Java to get player position
    // In practice, the native client would track this via hooks
    return 0.0;
}

JNIEXPORT jdouble JNICALL Java_com_utilitymod_nativeclient_NativeBridge_getPlayerY(JNIEnv* env, jclass clazz) {
    return 0.0;
}

JNIEXPORT jdouble JNICALL Java_com_utilitymod_nativeclient_NativeBridge_getPlayerZ(JNIEnv* env, jclass clazz) {
    return 0.0;
}

JNIEXPORT jfloat JNICALL Java_com_utilitymod_nativeclient_NativeBridge_getPlayerYaw(JNIEnv* env, jclass clazz) {
    return 0.0f;
}

JNIEXPORT jfloat JNICALL Java_com_utilitymod_nativeclient_NativeBridge_getPlayerPitch(JNIEnv* env, jclass clazz) {
    return 0.0f;
}

JNIEXPORT jboolean JNICALL Java_com_utilitymod_nativeclient_NativeBridge_isPlayerAlive(JNIEnv* env, jclass clazz) {
    return JNI_FALSE;
}

JNIEXPORT jint JNICALL Java_com_utilitymod_nativeclient_NativeBridge_getEntityId(JNIEnv* env, jclass clazz) {
    return -1;
}

} // extern "C"

// Exported C API for direct use (optional)
extern "C" {

UTILITYMOD_API bool UtilityMod_Initialize() {
    std::call_once(initFlag, []() {
        NativeConfig config;
        config.enableImgui = true;
        config.enableHooks = true;
        config.enableJniBridge = true;
        NativeClient::instance().initialize(config);
    });
    return NativeClient::instance().isInitialized();
}

UTILITYMOD_API void UtilityMod_Shutdown() {
    NativeClient::instance().shutdown();
}

UTILITYMOD_API bool UtilityMod_IsLoaded() {
    return NativeClient::instance().isInitialized();
}

UTILITYMOD_API void UtilityMod_SetModuleEnabled(int moduleId, bool enabled) {
    NativeClient::instance().setModuleState(
        static_cast<ModuleId>(moduleId),
        enabled
    );
}

UTILITYMOD_API bool UtilityMod_IsModuleEnabled(int moduleId) {
    return NativeClient::instance().getModuleState(
        static_cast<ModuleId>(moduleId)
    ).enabled;
}

UTILITYMOD_API void UtilityMod_ToggleMenu() {
    if (NativeClient::instance().getImGui()) {
        NativeClient::instance().getImGui()->toggleMenu();
    }
}

} // extern "C"

} // namespace utilitymod
