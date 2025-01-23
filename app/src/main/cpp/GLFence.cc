//
// Created by huang on 2025-01-22.
//

#include "GLFence.h"

#include "Log.h"

#define LOG_TAG "SurfaceControlApp"

PFNEGLDUPNATIVEFENCEFDANDROIDPROC eglDupNativeFenceFDANDROIDFn = nullptr;

GLFence::GLFence() = default;

GLFence::~GLFence() {
    if (mSync != EGL_NO_SYNC_KHR) {
        eglDestroySyncKHR(eglGetCurrentDisplay(), mSync);
    }
}

// static
std::shared_ptr<GLFence> GLFence::Create() {
    auto fence = std::make_shared<GLFence>();
    if (!fence->init(EGL_SYNC_FENCE_KHR, nullptr)) {
        return nullptr;
    }
    return fence;
}

// static
std::shared_ptr<GLFence> GLFence::CreateFromFenceFd(int fenceFd) {
    EGLint attribs[] = {EGL_SYNC_NATIVE_FENCE_FD_ANDROID, fenceFd, EGL_NONE};
    auto fence = std::make_shared<GLFence>();
    if (!fence->init(EGL_SYNC_FENCE_KHR, attribs)) {
        return nullptr;
    }
    return fence;
}

bool GLFence::init(EGLint type, const EGLint *attribs) {
    mSync = eglCreateSyncKHR(eglGetCurrentDisplay(), type, attribs);
    if (mSync == EGL_NO_SYNC_KHR) {
        LOGE("Failed to eglCreateSyncKHR");
        return false;
    }
    return true;
}

void GLFence::wait() {
    eglWaitSyncKHR(eglGetCurrentDisplay(), mSync, 0);
}

int GLFence::getFd() {
    if (eglDupNativeFenceFDANDROIDFn == nullptr) {
        eglDupNativeFenceFDANDROIDFn = reinterpret_cast<PFNEGLDUPNATIVEFENCEFDANDROIDPROC>(
                eglGetProcAddress("eglDupNativeFenceFDANDROID"));
        if (eglDupNativeFenceFDANDROIDFn == nullptr) {
            LOGE("Failed to eglGetProcAddress eglDupNativeFenceFDANDROID");
            return -1;
        }
    }
    EGLint fd = eglDupNativeFenceFDANDROIDFn(eglGetCurrentDisplay(), mSync);
    if (fd < 0) {
        LOGE("Failed to eglDupNativeFenceFDANDROID");
        return -1;
    }
    return fd;
}