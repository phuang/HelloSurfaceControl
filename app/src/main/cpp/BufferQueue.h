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

class GLFence;

struct AHardwareBufferDeleter {
    void operator()(AHardwareBuffer* buffer) const {
        AHardwareBuffer_release(buffer);
    }
};

using UniqueAHardwareBuffer = std::unique_ptr<AHardwareBuffer, AHardwareBufferDeleter>;

class BufferQueue {
public:
    explicit BufferQueue(VkDevice device);
    ~BufferQueue();

    void resize(int width, int height);

    void createBuffers();
    void releaseBuffers();

    struct Image {
        Image(AHardwareBuffer* buffer, EGLImage eglImage) : buffer(buffer), eglImage(eglImage) {}
        AHardwareBuffer* buffer = nullptr;
        EGLImage eglImage = EGL_NO_IMAGE;
        std::shared_ptr<GLFence> fence;
        int fenceFd = -1;
    };

    const Image* produceImage();
    void enqueueProducedImage(std::shared_ptr<GLFence> fence);

    const Image* presentImage();
    void releasePresentImage(int fenceFd);

private:
    VkDevice mDevice = VK_NULL_HANDLE;
    int mWidth = 0;
    int mHeight = 0;
    std::vector<UniqueAHardwareBuffer> mBuffers;
    std::vector<VkImage> mImages;
    std::vector<EGLImage> mEGLImages;

    std::mutex mMutex;
    std::deque<std::unique_ptr<Image>> mAvailableImages;
    std::deque<std::unique_ptr<Image>> mProducedImages;
    std::deque<std::unique_ptr<Image>> mInPresentImages;

    std::unique_ptr<Image> mCurrentProduceImage;
};


#endif //HELLOSURFACECONTROL_BUFFERQUEUE_H
