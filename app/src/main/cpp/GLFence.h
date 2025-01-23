//
// Created by huang on 2025-01-22.
//

#ifndef HELLOSURFACECONTROL_GLFENCE_H
#define HELLOSURFACECONTROL_GLFENCE_H

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <memory>

#include "ScopedFd.h"

class GLFence : public std::enable_shared_from_this<GLFence> {
public:
    GLFence();
    ~GLFence();

    static std::shared_ptr<GLFence> Create();
    static std::shared_ptr<GLFence> CreateFromFenceFd(ScopedFd fenceFd);

    void wait();
    ScopedFd getFd();

private:
    bool init(EGLint type, const EGLint *attribs);
    EGLSyncKHR mSync = EGL_NO_SYNC_KHR;
};


#endif //HELLOSURFACECONTROL_GLFENCE_H
