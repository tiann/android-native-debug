#include <jni.h>
#include <string>

extern "C"
jstring
Java_native_1debug_weishu_me_android_1native_1debug_MainActivity_stringFromJNI(
        JNIEnv *env,
        jobject /* this */) {
    std::string hello = "Hello from C++";
    return env->NewStringUTF(hello.c_str());
}
