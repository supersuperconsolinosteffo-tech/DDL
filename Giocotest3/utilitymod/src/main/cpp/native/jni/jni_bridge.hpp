// jni_bridge.hpp
// JNI/JVMTI bridge for communication with Java mod

#pragma once

#include <jni.h>
#include <jvmti.h>
#include <string>
#include <functional>
#include <mutex>
#include <atomic>
#include <thread>

namespace utilitymod {

class JniBridge {
public:
    JniBridge();
    ~JniBridge();

    // Initialize the JNI bridge, loading the JVM.
    // Only for a standalone process that does not have a JVM yet. When the
    // library is loaded by Minecraft via System.load(), use
    // attachToExistingVm() instead - creating a second JVM is not allowed.
    bool initialize(const std::string& jvmLibraryPath = "");

    // Adopt the JavaVM of the already-running JVM (the normal path when the
    // mod calls System.load on this library). Does not load or create a JVM.
    bool attachToExistingVm(JavaVM* vm);

    // Shutdown the JNI bridge
    void shutdown();

    // Check if initialized
    bool isInitialized() const { return jvm_ != nullptr && env_ != nullptr; }

    // Get JNIEnv for current thread (attaches if needed)
    JNIEnv* getEnv();

    // Get JavaVM
    JavaVM* getJvm() { return jvm_; }

    // Call a static Java method
    template<typename... Args>
    bool callStaticVoidMethod(const std::string& className, const std::string& methodName, const std::string& signature, Args... args) {
        JNIEnv* env = getEnv();
        if (!env) return false;

        jclass clazz = env->FindClass(className.c_str());
        if (!clazz) {
            log("Class not found: " + className);
            return false;
        }

        jmethodID method = env->GetStaticMethodID(clazz, methodName.c_str(), signature.c_str());
        if (!method) {
            log("Method not found: " + className + "." + methodName + signature);
            return false;
        }

        env->CallStaticVoidMethod(clazz, method, args...);
        return !env->ExceptionCheck();
    }

    // Call a static Java method returning jobject
    jobject callStaticObjectMethod(const std::string& className, const std::string& methodName, const std::string& signature, ...);

    // Find a class
    jclass findClass(const std::string& className);

    // Get a field ID
    jfieldID getFieldID(jclass clazz, const std::string& name, const std::string& signature);

    // Get a method ID
    jmethodID getMethodID(jclass clazz, const std::string& name, const std::string& signature);

    // Register native methods
    bool registerNatives(const std::string& className, const JNINativeMethod* methods, int count);

    // Set callback for Java -> Native log
    using JavaLogCallback = std::function<void(const std::string&)>;
    void setJavaLogCallback(JavaLogCallback cb) { javaLogCallback_ = std::move(cb); }

private:
    JavaVM* jvm_ = nullptr;
    JNIEnv* env_ = nullptr;
    jvmtiEnv* jvmti_ = nullptr;
    std::mutex envMutex_;
    std::thread::id mainThreadId_;

    JavaLogCallback javaLogCallback_;

    // Load JVM library
    bool loadJvm(const std::string& path);

    // Create JVM
    bool createJvm();

    // Attach current thread
    bool attachThread();

    // Detach current thread
    void detachThread();

    void log(const std::string& message);
};

} // namespace utilitymod