//
// Created by huang on 2025-01-20.
//

#include "BufferQueue.h"

#include <cassert>
#include <EGL/eglext.h>
#include <unistd.h>

#include "GLFence.h"
#include "Log.h"

#define LOG_TAG "SurfaceControlApp"

constexpr int kBufferCount = 3;

BufferQueue::BufferQueue(VkDevice device) : mDevice(device) {}

BufferQueue::~BufferQueue() {
    releaseBuffers();
}

void BufferQueue::createBuffers() {
    assert(mBuffers.empty());
    assert(mImages.empty());

    // Create buffers
    for (int i = 0; i < kBufferCount; i++) {
        AHardwareBuffer *buffer = nullptr;
        AHardwareBuffer_Desc desc = {};
        desc.width = mWidth;
        desc.height = mHeight;
        desc.layers = 1;
        desc.format = AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM;
        desc.usage =
                AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE | AHARDWAREBUFFER_USAGE_GPU_FRAMEBUFFER;
        AHardwareBuffer_allocate(&desc, &buffer);
        mBuffers.emplace_back(buffer);

        // Import the AHardwareBuffer to the EGLImage
        EGLClientBuffer clientBuffer = eglGetNativeClientBufferANDROID(buffer);
        EGLImage eglImage = eglCreateImageKHR(eglGetCurrentDisplay(), EGL_NO_CONTEXT,
                                              EGL_NATIVE_BUFFER_ANDROID,
                                              clientBuffer, nullptr);
        if (eglImage == EGL_NO_IMAGE_KHR) {
            LOGE("Failed to create EGL image");
            return;
        }
        mEGLImages.push_back(eglImage);

        mAvailableImages.push_back({buffer, eglImage});

        // Create a VkImage with external memory support
        VkExternalMemoryImageCreateInfo externalMemoryImageCreateInfo = {};
        externalMemoryImageCreateInfo.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
        externalMemoryImageCreateInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID;

        VkImageCreateInfo imageCreateInfo = {};
        imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageCreateInfo.pNext = &externalMemoryImageCreateInfo;
        imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
        imageCreateInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        imageCreateInfo.extent = {static_cast<uint32_t>(desc.width),
                                  static_cast<uint32_t>(desc.height), 1};
        imageCreateInfo.mipLevels = 1;
        imageCreateInfo.arrayLayers = 1;
        imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageCreateInfo.usage =
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VkImage image = VK_NULL_HANDLE;
        VkResult result = vkCreateImage(mDevice, &imageCreateInfo, nullptr, &image);
        if (result != VK_SUCCESS) {
            LOGE("Failed to create image");
            return;
        }

        // Import the AHardwareBuffer to the VkImage
        VkImportAndroidHardwareBufferInfoANDROID importInfo = {};
        importInfo.sType = VK_STRUCTURE_TYPE_IMPORT_ANDROID_HARDWARE_BUFFER_INFO_ANDROID;
        importInfo.buffer = buffer;

        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(mDevice, image, &memRequirements);

        VkMemoryAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = 0; // You need to find the proper memory type index

        VkMemoryDedicatedAllocateInfo dedicatedAllocInfo = {};
        dedicatedAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
        dedicatedAllocInfo.image = image;

        allocInfo.pNext = &dedicatedAllocInfo;

        VkDeviceMemory memory;
        result = vkAllocateMemory(mDevice, &allocInfo, nullptr, &memory);
        if (result != VK_SUCCESS) {
            LOGE("Failed to allocate memory");
            return;
        }

        result = vkBindImageMemory(mDevice, image, memory, 0);
        if (result != VK_SUCCESS) {
            LOGE("Failed to bind image memory");
            return;
        }

        mImages.push_back(image);
    }
}

void BufferQueue::releaseBuffers() {
    // Release old egl images
    for (auto eglImage: mEGLImages) {
        eglDestroyImageKHR(eglGetCurrentDisplay(), eglImage);
    }
    mEGLImages.clear();

    // Release old images
    for (auto image: mImages) {
        vkDestroyImage(mDevice, image, nullptr);
    }
    mImages.clear();

    mBuffers.clear();
}

void BufferQueue::resize(int width, int height) {
    if (mWidth == width && mHeight == height) {
        return;
    }

    mWidth = width;
    mHeight = height;

    releaseBuffers();
    createBuffers();
}

BufferQueue::Image BufferQueue::produceImage() {
    std::unique_lock<std::mutex> lock(mMutex);
    assert(mCurrentProduceImage.buffer == nullptr);
    if (mAvailableImages.empty()) {
        return {};
    }
    mCurrentProduceImage = mAvailableImages.front();
    mAvailableImages.pop_front();
    if (mCurrentProduceImage.fenceFd >= 0) {
        mCurrentProduceImage.fence = GLFence::CreateFromFenceFd(mCurrentProduceImage.fenceFd);
    }
    return mCurrentProduceImage;
}

void BufferQueue::enqueueProducedImage(std::shared_ptr<GLFence> fence) {
    std::unique_lock<std::mutex> lock(mMutex);
    assert(mCurrentProduceImage.buffer != nullptr);
    mProducedImages.push_back(mCurrentProduceImage);
    mCurrentProduceImage.fence = std::move(fence);
    mCurrentProduceImage = {};
}

BufferQueue::Image BufferQueue::presentImage() {
    std::unique_lock<std::mutex> lock(mMutex);
    if (mProducedImages.empty()) {
        return {};
    }
    auto image = mProducedImages.front();
    mInPresentImages.push_back(image);
    return image;
}

void BufferQueue::releasePresentImage(int fenceFd) {
    std::unique_lock<std::mutex> lock(mMutex);
    assert(!mInPresentImages.empty());
    auto image = mInPresentImages.front();
    mInPresentImages.pop_front();
    mAvailableImages.push_back(image);
    mAvailableImages.back().fence = nullptr;
    // releaseProducerFence(fenceFd) could be called off gl thread, fence fd cannot be imported off
    // the gl thread, so we have to defer importing the fence.
    mAvailableImages.back().fenceFd = fenceFd;
}
