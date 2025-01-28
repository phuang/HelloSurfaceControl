#include <jni.h>
#include <android/native_window_jni.h>
#include <memory>
#include <string>

#include "HelloSurfaceControl.h"
#include "Log.h"

#define LOG_TAG "SurfaceControlApp"

static std::unique_ptr<HelloSurfaceControl> gHelloSurfaceControl;

extern "C" JNIEXPORT void JNICALL
Java_com_example_hellosurfacecontrol_MainActivity_nativeInitSurfaceControl(
        JNIEnv* env,
        jobject /* this */,
        jobject surface) {
    assert(!gHelloSurfaceControl);
    gHelloSurfaceControl = std::make_unique<HelloSurfaceControl>();

    ANativeWindow* window = ANativeWindow_fromSurface(env, surface);
    if (window == nullptr) {
        LOGE("Failed to get native window from surface");
        return;
    }

    if (!gHelloSurfaceControl->init(window)) {
        LOGE("Failed to init HelloSurfaceControl");
        return;
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_com_example_hellosurfacecontrol_MainActivity_nativeUpdateSurfaceControl(JNIEnv *env,
                                                                             jobject thiz,
                                                                             jobject surface,
                                                                             jint format,
                                                                             jint width,
                                                                             jint height) {
    assert(gHelloSurfaceControl);
    gHelloSurfaceControl->update(format, width, height);
}
extern "C"
JNIEXPORT void JNICALL
Java_com_example_hellosurfacecontrol_MainActivity_nativeDestroySurfaceControl(JNIEnv *env,
                                                                              jobject thiz,
                                                                              jobject surface) {
    assert(gHelloSurfaceControl);
    gHelloSurfaceControl = nullptr;
}