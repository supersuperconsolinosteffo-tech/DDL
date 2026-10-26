// jni_bridge.cpp
// JNI/JVMTI bridge implementation

#include "jni_bridge.hpp"
#include "native_client.hpp"

#include <cstdarg>
#include <iostream>
#include <filesystem>

#if defined(_WIN32)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
#else
    #include <dlfcn.h>
#endif

#if defined(_WIN32)
// Handle of the JVM library loaded in loadJvm(); kept alive so that the
// JNI_CreateJavaVM / JNI_GetCreatedJavaVMs entry points resolved from it stay
// valid. We do not link against jvm.lib: the DLL must be locatable at runtime
// instead of at link time.
static HMODULE g_jvmLibrary = nullptr;

namespace {
using JniCreateJavaVmFn = jint (JNICALL*)(JavaVM**, void**, void*);
using JniGetCreatedJavaVmsFn = jint (JNICALL*)(JavaVM**, jsize, jsize*);

void* resolveJvmSymbol(const char* name) {
    if (!g_jvmLibrary) return nullptr;
    return reinterpret_cast<void*>(GetProcAddress(g_jvmLibrary, name));
}
} // namespace
#endif

namespace utilitymod {

JniBridge::JniBridge() : mainThreadId_(std::this_thread::get_id()) {}

JniBridge::~JniBridge() {
    shutdown();
}

bool JniBridge::initialize(const std::string& jvmLibraryPath) {
    if (jvm_) return true;

    if (!loadJvm(jvmLibraryPath)) {
        return false;
    }

    if (!createJvm()) {
        return false;
    }

    // Get JVMTI environment
    jint result = jvm_->GetEnv(reinterpret_cast<void**>(&jvmti_), JVMTI_VERSION_1_0);
    if (result != JNI_OK) {
        log("Failed to get JVMTI environment: " + std::to_string(result));
        // JVMTI is optional, continue without it
    }

    log("JNI bridge initialized successfully");
    return true;
}

void JniBridge::shutdown() {
    if (jvm_) {
        jvm_->DestroyJavaVM();
        jvm_ = nullptr;
        env_ = nullptr;
        jvmti_ = nullptr;
    }
}

bool JniBridge::loadJvm(const std::string& jvmLibraryPath) {
    std::string libPath;

    if (!jvmLibraryPath.empty()) {
        libPath = jvmLibraryPath;
    } else {
        // Try to find JVM automatically
#if defined(_WIN32)
        // Check common Windows locations
        std::vector<std::string> searchPaths = {
            "C:\\Program Files\\Java\\jre*\\bin\\server\\jvm.dll",
            "C:\\Program Files\\Java\\jdk*\\jre\\bin\\server\\jvm.dll",
            "C:\\Program Files\\Eclipse Adoptium\\jre*\\bin\\server\\jvm.dll",
            "C:\\Program Files\\Eclipse Adoptium\\jdk*\\jre\\bin\\server\\jvm.dll"
        };

        for (const auto& pattern : searchPaths) {
            for (const auto& entry : std::filesystem::directory_iterator(std::filesystem::path(pattern).parent_path())) {
                if (entry.path().filename() == "jvm.dll") {
                    libPath = entry.path().string();
                    break;
                }
            }
            if (!libPath.empty()) break;
        }
#else
        // Linux/macOS - use dlopen with default path
        libPath = "libjvm.so";
#endif
    }

    if (libPath.empty()) {
        log("Could not find JVM library automatically");
        return false;
    }

#if defined(_WIN32)
    HMODULE handle = LoadLibraryA(libPath.c_str());
    if (!handle) {
        log("Failed to load JVM: " + libPath + " (error: " + std::to_string(GetLastError()) + ")");
        return false;
    }
    g_jvmLibrary = handle;
#else
    void* handle = dlopen(libPath.c_str(), RTLD_LAZY | RTLD_GLOBAL);
    if (!handle) {
        log("Failed to load JVM: " + libPath + " (error: " + dlerror() + ")");
        return false;
    }
#endif

    log("Loaded JVM from: " + libPath);
    return true;
}

bool JniBridge::createJvm() {
    JavaVMInitArgs vm_args;
    JavaVMOption options[4];
    int optionCount = 0;

    // Classpath - we'll use the existing JVM's classpath
    options[optionCount++].optionString = const_cast<char*>("-Djava.class.path=.");

    // Enable JVMTI
    options[optionCount++].optionString = const_cast<char*>("-agentlib:jvmti");

    // Headless mode (optional)
    // options[optionCount++].optionString = const_cast<char*>("-Djava.awt.headless=true");

    vm_args.version = JNI_VERSION_1_8;
    vm_args.nOptions = optionCount;
    vm_args.options = options;
    vm_args.ignoreUnrecognized = JNI_TRUE;

#if defined(_WIN32)
    // Resolved dynamically so that the build does not require jvm.lib.
    auto createJavaVM = reinterpret_cast<JniCreateJavaVmFn>(resolveJvmSymbol("JNI_CreateJavaVM"));
    if (!createJavaVM) {
        log("JNI_CreateJavaVM not found in loaded JVM library");
        return false;
    }
    jint result = createJavaVM(&jvm_, reinterpret_cast<void**>(&env_), &vm_args);
#else
    jint result = JNI_CreateJavaVM(&jvm_, reinterpret_cast<void**>(&env_), &vm_args);
#endif
    if (result != JNI_OK) {
        log("Failed to create JVM: " + std::to_string(result));
        return false;
    }

    // We're attaching to the existing Minecraft JVM, not creating a new one
    // In practice, we get the JVM from the injected process
    log("Note: In production, we attach to existing Minecraft JVM via JVMTI");
    return true;
}

JNIEnv* JniBridge::getEnv() {
    std::lock_guard<std::mutex> lock(envMutex_);

    if (std::this_thread::get_id() == mainThreadId_ && env_) {
        return env_;
    }

    if (!jvm_) return nullptr;

    JNIEnv* env = nullptr;
    jint result = jvm_->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_8);
    if (result == JNI_EDETACHED) {
        if (attachThread()) {
            result = jvm_->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_8);
        }
    }

    if (result != JNI_OK || !env) {
        return nullptr;
    }

    if (std::this_thread::get_id() == mainThreadId_) {
        env_ = env;
    }

    return env;
}

bool JniBridge::attachThread() {
    JavaVMAttachArgs args;
    args.version = JNI_VERSION_1_8;
    args.name = const_cast<char*>("UtilityMod-Native");
    args.group = nullptr;

    jint result = jvm_->AttachCurrentThread(reinterpret_cast<void**>(&env_), &args);
    return result == JNI_OK;
}

void JniBridge::detachThread() {
    if (jvm_ && std::this_thread::get_id() != mainThreadId_) {
        jvm_->DetachCurrentThread();
    }
}

jclass JniBridge::findClass(const std::string& className) {
    JNIEnv* env = getEnv();
    if (!env) return nullptr;

    jclass clazz = env->FindClass(className.c_str());
    if (!clazz) {
        log("Class not found: " + className);
    }
    return clazz;
}

jfieldID JniBridge::getFieldID(jclass clazz, const std::string& name, const std::string& signature) {
    JNIEnv* env = getEnv();
    if (!env || !clazz) return nullptr;

    jfieldID field = env->GetFieldID(clazz, name.c_str(), signature.c_str());
    if (!field) {
        log("Field not found: " + name + " " + signature);
    }
    return field;
}

jmethodID JniBridge::getMethodID(jclass clazz, const std::string& name, const std::string& signature) {
    JNIEnv* env = getEnv();
    if (!env || !clazz) return nullptr;

    jmethodID method = env->GetMethodID(clazz, name.c_str(), signature.c_str());
    if (!method) {
        method = env->GetStaticMethodID(clazz, name.c_str(), signature.c_str());
    }
    if (!method) {
        log("Method not found: " + name + " " + signature);
    }
    return method;
}

bool JniBridge::registerNatives(const std::string& className, const JNINativeMethod* methods, int count) {
    JNIEnv* env = getEnv();
    if (!env) return false;

    jclass clazz = findClass(className);
    if (!clazz) return false;

    jint result = env->RegisterNatives(clazz, methods, count);
    if (result != JNI_OK) {
        log("Failed to register natives for: " + className);
        return false;
    }

    return true;
}

jobject JniBridge::callStaticObjectMethod(const std::string& className, const std::string& methodName, const std::string& signature, ...) {
    JNIEnv* env = getEnv();
    if (!env) return nullptr;

    jclass clazz = findClass(className);
    if (!clazz) return nullptr;

    jmethodID method = env->GetStaticMethodID(clazz, methodName.c_str(), signature.c_str());
    if (!method) {
        log("Static method not found: " + className + "." + methodName + signature);
        return nullptr;
    }

    va_list args;
    va_start(args, signature);
    jobject result = env->CallStaticObjectMethodV(clazz, method, args);
    va_end(args);

    if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
        return nullptr;
    }

    return result;
}

void JniBridge::log(const std::string& message) {
    if (javaLogCallback_) {
        javaLogCallback_(message);
    }
    std::cout << "[JNI] " << message << std::endl;
}

} // namespace utilitymod