//
// Created by huang on 2025-01-20.
//

#ifndef HELLOSURFACECONTROL_HELLOSURFACECONTROL_H
#define HELLOSURFACECONTROL_HELLOSURFACECONTROL_H

#include <android/surface_control.h>
#include <vulkan/vulkan.h>
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>
#include <EGL/egl.h>

#include <condition_variable>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

#include "ChildSurface.h"

class HelloSurfaceControl {
public:
    HelloSurfaceControl();
    ~HelloSurfaceControl();

    bool init(ANativeWindow* window);
    void update(int format, int width, int height);

private:
    void runOnRT();

    bool initEGLOnRT();
    bool initVulkanOnRT();
    bool initOnRT(ANativeWindow* window);
    void releaseOnRT();
    void updateOnRT(int format, int width, int height);
    void drawOnRT();

    std::mutex mMutex;
    std::condition_variable mCondition;

    EGLDisplay mEGLDisplay = EGL_NO_DISPLAY;
    EGLContext mEGLContext = EGL_NO_CONTEXT;

    VkInstance mInstance = VK_NULL_HANDLE;
    VkDevice mDevice = VK_NULL_HANDLE;
    VkQueue mQueue = VK_NULL_HANDLE;

    struct ANativeWindowDeleter {
        void operator()(ANativeWindow* window) const {
            ANativeWindow_release(window);
        }
    };
    std::unique_ptr<ANativeWindow, ANativeWindowDeleter> mWindow;
    UniqueASurfaceControl mSurfaceControl;

    int mWidth = 0;
    int mHeight = 0;

    std::vector<std::shared_ptr<ChildSurface>> mChildSurfaces;

    bool mBeingDestroyed = false;
    bool mReadyToDraw = false;
    uint32_t mFrameCount = 0;

    std::deque<std::function<void()>> mTasks;
    std::optional<std::thread> mThread;
};


#endif //HELLOSURFACECONTROL_HELLOSURFACECONTROL_H
