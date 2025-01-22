//
// Created by huang on 2025-01-20.
//

#include "HelloSurfaceControl.h"

#include <android/native_window.h>
#include "Log.h"

#define LOG_TAG "SurfaceControlApp"

constexpr int kChildrenCount = 4;
constexpr int kChildSize = 800;

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

    int x = 0;
    int y = 0;
    float delta = 1.0f;
    for (int i = 0; i < kChildrenCount; i++) {
        mChildSurfaces.emplace_back(std::make_shared<ChildSurface>(mDevice, mQueue));
        mChildSurfaces.back()->init(mSurfaceControl, "HelloSurfaceControlChild");
        mChildSurfaces.back()->resize(kChildSize, kChildSize);
        mChildSurfaces.back()->setPosition(x, y);
        mChildSurfaces.back()->setAnimationDelta(delta);

        x += 80;
        y += 500;
        delta *= 1.5f;
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

    mReadyToDraw = true;
}

void HelloSurfaceControl::update(int format, int width, int height) {
    std::unique_lock<std::mutex> lock(mMutex);
    mTasks.emplace_back([this, format, width, height] { updateOnRT(format, width, height); });
    mCondition.notify_one();
}

void HelloSurfaceControl::drawOnRT() {
    ASurfaceTransaction *transaction = ASurfaceTransaction_create();

    const float kAnimationPeriod = 200.0f;
    float factor = std::abs(
            .5f - (mFrameCount % static_cast<uint32_t>(kAnimationPeriod)) / kAnimationPeriod);
    {
        float scale = factor + 0.5f;
        mChildSurfaces[0]->setScale(scale, scale);
    }
    {
        int crop = 200.0f * factor;
        mChildSurfaces[1]->setCrop({crop, crop, kChildSize - crop * 2, kChildSize - crop * 2});
    }
    {
        float scale = factor * 2;
        mChildSurfaces[2]->setAlpha(scale);
    }
    {
        int x = 600 * factor + 200;
        int y = 600 * factor + 1100;
        mChildSurfaces[3]->setPosition(x, y);
    }

    for (auto &childSurface: mChildSurfaces) {
        childSurface->draw();
        childSurface->applyChanges(transaction);
    }

    glFinish();
    ASurfaceTransaction_apply(transaction);
    ASurfaceTransaction_delete(transaction);
    mFrameCount++;
}

void HelloSurfaceControl::releaseOnRT() {
    mChildSurfaces.clear();

    if (mSurfaceControl) {
        ASurfaceControl_release(mSurfaceControl);
        mSurfaceControl = nullptr;
    }
    if (mWindow) {
        ANativeWindow_release(mWindow);
        mWindow = nullptr;
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

