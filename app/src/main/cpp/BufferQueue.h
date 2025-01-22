//
// Created by huang on 2025-01-20.
//

#ifndef HELLOSURFACECONTROL_BUFFERQUEUE_H
#define HELLOSURFACECONTROL_BUFFERQUEUE_H


#include <android/hardware_buffer.h>

#include <EGL/egl.h>
#include <vulkan/vulkan.h>

#include <deque>
#include <mutex>
#include <vector>

class BufferQueue {
public:
    explicit BufferQueue(VkDevice device);
    ~BufferQueue();

    void resize(int width, int height);

    void createBuffers();
    void releaseBuffers();

    struct Image {
        AHardwareBuffer* buffer = nullptr;
        EGLImage eglImage = EGL_NO_IMAGE;
    };

    Image produceImage();
    void enqueueProducedImage();

    Image presentImage();
    void releasePresentImage(int fenceFd);

private:
    VkDevice mDevice = VK_NULL_HANDLE;
    int mWidth = 0;
    int mHeight = 0;
    std::vector<AHardwareBuffer*> mBuffers;
    std::vector<VkImage> mImages;
    std::vector<EGLImage> mEGLImages;

    std::mutex mMutex;
    std::deque<Image> mAvailableImages;
    std::deque<Image> mProducedImages;
    std::deque<Image> mInPresentImages;

    Image mCurrentProduceImage = {};
};


#endif //HELLOSURFACECONTROL_BUFFERQUEUE_H
