//
// Created by huang on 2025-01-20.
//

#include "BufferQueue.h"

#include <assert.h>
#include <EGL/eglext.h>

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
        desc.usage = AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE | AHARDWAREBUFFER_USAGE_GPU_FRAMEBUFFER;
        AHardwareBuffer_allocate(&desc, &buffer);
        mBuffers.push_back(buffer);

        // Import the AHardwareBuffer to the EGLImage
        EGLClientBuffer clientBuffer = eglGetNativeClientBufferANDROID(buffer);
        EGLImage eglImage = eglCreateImageKHR(eglGetCurrentDisplay(), EGL_NO_CONTEXT, EGL_NATIVE_BUFFER_ANDROID,
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

    // Release old buffers
    for (auto buffer: mBuffers) {
        AHardwareBuffer_release(buffer);
    }
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

void BufferQueue::produceImage() {
    assert(!mAvailableImages.empty());
    assert(mCurrentProduceImage.buffer == nullptr);
    mCurrentProduceImage = mAvailableImages.front();
    mAvailableImages.pop_front();
}

void BufferQueue::enqueueProducedImage() {
    assert(mCurrentProduceImage.buffer != nullptr);
    mProducedImages.push_back(mCurrentProduceImage);
    mCurrentProduceImage = {};
}

BufferQueue::Image BufferQueue::presentImage() {
    assert(!mProducedImages.empty());
    auto image = mProducedImages.front();
    mInPresentImages.push_back(image);
    return image;
}

void BufferQueue::releasePresentImage() {
    assert(!mInPresentImages.empty());
    auto image = mInPresentImages.front();
    mInPresentImages.pop_front();
    mAvailableImages.push_back(image);
}