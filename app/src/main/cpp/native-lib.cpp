#include <jni.h>
#include <string>
#include <android/log.h>
#include <android/native_window_jni.h>
#include <android/surface_control.h>

#define LOG_TAG "SurfaceControlApp"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

extern "C" JNIEXPORT void JNICALL
Java_com_example_hellosurfacecontrol_MainActivity_nativeInitSurfaceControl(
        JNIEnv* env,
        jobject /* this */,
        jobject surface) {
    ANativeWindow* window = ANativeWindow_fromSurface(env, surface);
    if (window == nullptr) {
        LOGE("Failed to get native window from surface");
        return;
    }

    // Example of using Surface Control
    ASurfaceControl* surfaceControl = ASurfaceControl_createFromWindow(window, "ExampleSurfaceControl");
    if (surfaceControl == nullptr) {
        LOGE("Failed to create Surface Control");
        return;
    }

    // Perform operations with surfaceControl...

    // Clean up
    ASurfaceControl_release(surfaceControl);
    ANativeWindow_release(window);
}