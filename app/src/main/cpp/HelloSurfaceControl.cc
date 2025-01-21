//
// Created by huang on 2025-01-20.
//

#include "HelloSurfaceControl.h"

#include <android/native_window.h>
#include "Log.h"

#define LOG_TAG "SurfaceControlApp"

constexpr int kChildrenCount = 4;

HelloSurfaceControl::HelloSurfaceControl() {
    mThread.emplace([this] { runOnRT(); });
}

HelloSurfaceControl::~HelloSurfaceControl() {
    LOGD("HelloSurfaceControl::~HelloSurfaceControl()");
    {
        std::unique_lock<std::mutex> lock(mMutex);
        mBeingDestroyed = true;
        mCondition.notify_one();
    }
    if (mThread) {
        mThread->join();
    }
}

bool HelloSurfaceControl::initEGLOnRT() {
    // Init EGL
    mEGLDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (mEGLDisplay == EGL_NO_DISPLAY) {
        LOGE("Failed to get EGL display");
        return false;
    }

    if (!eglInitialize(mEGLDisplay, nullptr, nullptr)) {
        LOGE("Failed to initialize EGL");
        return false;
    }

    EGLConfig config;
    EGLint numConfigs;
    EGLint configAttribs[] = {
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
            EGL_NONE
    };
    if (!eglChooseConfig(mEGLDisplay, configAttribs, &config, 1, &numConfigs)) {
        LOGE("Failed to choose EGL config");
        return false;
    }

    EGLint contextAttribs[] = {
            EGL_CONTEXT_CLIENT_VERSION, 3,
            EGL_NONE
    };

    mEGLContext = eglCreateContext(mEGLDisplay, config, EGL_NO_CONTEXT, contextAttribs);
    if (mEGLContext == EGL_NO_CONTEXT) {
        LOGE("Failed to create EGL context");
        return false;
    }

    if (!eglMakeCurrent(mEGLDisplay, EGL_NO_CONTEXT, EGL_NO_CONTEXT, mEGLContext)) {
        LOGE("Failed to make EGL context current");
        return false;
    }

    return true;
}

bool HelloSurfaceControl::initVulkanOnRT() {
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "HelloSurfaceControl";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "HelloSurfaceControl";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    VkResult result = vkCreateInstance(&createInfo, nullptr, &mInstance);
    if (result != VK_SUCCESS) {
        LOGE("Failed to create Vulkan instance");
        return false;
    }

    // Enumerate physical devices
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(mInstance, &deviceCount, nullptr);
    if (deviceCount == 0) {
        LOGE("Failed to find GPUs with Vulkan support");
        return false;
    }

    std::vector<VkPhysicalDevice> physicalDevices(deviceCount);
    vkEnumeratePhysicalDevices(mInstance, &deviceCount, physicalDevices.data());

    // Select the first physical device
    VkPhysicalDevice physicalDevice = physicalDevices[0];

    // Create logical device
    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo = {};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = 0; // Assuming queue family index 0 supports the required operations
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    VkDeviceCreateInfo deviceCreateInfo = {};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;

    result = vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &mDevice);
    if (result != VK_SUCCESS) {
        LOGE("Failed to create Vulkan device");
        return false;
    }

    // Retrieve the queue
    vkGetDeviceQueue(mDevice, 0, 0, &mQueue);

    LOGD("Vulkan initialized");
    return true;
}

bool HelloSurfaceControl::initOnRT(ANativeWindow *window) {
    LOGD("HelloSurfaceControl::initOnRT()");

    if (!initVulkanOnRT()) {
        return false;
    }

    if (!initEGLOnRT()) {
        return false;
    }

    assert(mWindow == nullptr);
    assert(mSurfaceControl == nullptr);

    mWindow = window;
    mSurfaceControl = ASurfaceControl_createFromWindow(mWindow, "HelloSurfaceControl");
    if (mSurfaceControl == nullptr) {
        LOGE("Failed to create ASurfaceControl from ANativeWindow");
        return false;
    }

    for (int i = 0; i < kChildrenCount; i++) {
        mChildSurfaces.emplace_back(std::make_unique<ChildSurface>(mDevice, mQueue));
        mChildSurfaces.back()->init(mSurfaceControl, "HelloSurfaceControlChild");
        mChildSurfaces.back()->resize(800, 800);
    }

    return true;
}

bool HelloSurfaceControl::init(ANativeWindow *window) {
    std::unique_lock<std::mutex> lock(mMutex);
    mTasks.emplace_back([this, window] { initOnRT(window); });
    mCondition.notify_one();
    return true;
}


void HelloSurfaceControl::updateOnRT(int format, int width, int height) {
    LOGD("HelloSurfaceControl::updateOnRT(format=%d, width=%d, height=%d)", format, width, height);
    if (mWidth == width && mHeight == height) {
        return;
    }

    mWidth = width;
    mHeight = height;

//    for (auto &childSurface: mChildSurfaces) {
//        childSurface->resize(format, width, height / kChildrenCount);
//    }

    mReadyToDraw = true;
}

void HelloSurfaceControl::update(int format, int width, int height) {
    std::unique_lock<std::mutex> lock(mMutex);
    mTasks.emplace_back([this, format, width, height] { updateOnRT(format, width, height); });
    mCondition.notify_one();
}

void HelloSurfaceControl::drawOnRT() {
    static bool sFirstFrame = true;
    if (sFirstFrame) {
        LOGD("HelloSurfaceControl::drawOnRT()");
        ASurfaceTransaction *t = ASurfaceTransaction_create();
//        ASurfaceTransaction_setVisibility(t, mSurfaceControl, ASURFACE_TRANSACTION_VISIBILITY_SHOW);
//        ARect rect = {0, 0, mWidth, mHeight};
//        ASurfaceTransaction_setGeometry(t, mSurfaceControl, rect, rect, 0);
        static int kTransforms[] = {
                ANATIVEWINDOW_TRANSFORM_IDENTITY,
                ANATIVEWINDOW_TRANSFORM_ROTATE_90,
                ANATIVEWINDOW_TRANSFORM_ROTATE_180,
                ANATIVEWINDOW_TRANSFORM_ROTATE_270
        };
        int32_t top = 0;
        int32_t left = 0;
        float alpha = 1.0f;
        for (size_t i = 0; i < mChildSurfaces.size(); ++i) {
            auto &childSurface = mChildSurfaces[i];
            childSurface->draw();
            ASurfaceTransaction_setBuffer(t, childSurface->getSurfaceControl(),
                                          childSurface->getCurrentBuffer(), -1);
            ASurfaceTransaction_setBufferTransform(t, childSurface->getSurfaceControl(),
                                                   kTransforms[i % std::size(kTransforms)]);
            ASurfaceTransaction_setBufferAlpha(t, childSurface->getSurfaceControl(), alpha);
            ASurfaceTransaction_setVisibility(t, childSurface->getSurfaceControl(),
                                              ASURFACE_TRANSACTION_VISIBILITY_SHOW);
            ASurfaceTransaction_setPosition(t, childSurface->getSurfaceControl(), left, top);
            ASurfaceTransaction_setScale(t, childSurface->getSurfaceControl(), 1.0f, 1.0f);
            ASurfaceTransaction_setCrop(t, childSurface->getSurfaceControl(),
                                        {0, 0, childSurface->getWidth() * 2 / 3,
                                         childSurface->getHeight() * 2 / 3});
            left += 100;
            top += mHeight / kChildrenCount;
            alpha *= 0.8f;
        }
        glFinish();
        ASurfaceTransaction_apply(t);
        ASurfaceTransaction_delete(t);
        sFirstFrame = false;
    }
}

void HelloSurfaceControl::releaseOnRT() {
    mChildSurfaces.clear();

    if (mSurfaceControl) {
        ASurfaceControl_release(mSurfaceControl);
    }
    if (mWindow) {
        ANativeWindow_release(mWindow);
    }
}

void HelloSurfaceControl::runOnRT() {
    LOGD("HelloSurfaceControl::runOnRT()");
    std::unique_lock<std::mutex> lock(mMutex);
    while (!mBeingDestroyed) {
        while (!mTasks.empty()) {
            auto task = mTasks.front();
            mTasks.pop_front();
            lock.unlock();
            task();
            lock.lock();
        }
        if (mReadyToDraw) {
            lock.unlock();
            drawOnRT();
            lock.lock();
        }
        mCondition.wait_for(lock, std::chrono::milliseconds(16));
    }

    releaseOnRT();
}

