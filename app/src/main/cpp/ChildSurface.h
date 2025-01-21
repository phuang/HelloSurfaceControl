//
// Created by huang on 2025-01-20.
//

#ifndef HELLOSURFACECONTROL_CHILDSURFACE_H
#define HELLOSURFACECONTROL_CHILDSURFACE_H

#include <android/surface_control.h>
#include <GLES3/gl3.h>
#include <deque>

#include "BufferQueue.h"

class ChildSurface {
public:
    ChildSurface(VkDevice device, VkQueue queue);
    ~ChildSurface();

    bool init(ASurfaceControl* parent, const char* debugName);
    void resize(int width, int height);
    void draw();

    int getWidth() const {
        return mWidth;
    }

    int getHeight() const {
        return mHeight;
    }

    ASurfaceControl *getSurfaceControl() {
        return mSurfaceControl;
    }

    AHardwareBuffer* getCurrentBuffer() {
        return mBufferQueue.getCurrentBuffer();
    }

private:
    void drawGL();
    void createProgram();
    void setupBuffers();

    VkDevice mDevice = VK_NULL_HANDLE;
    VkQueue mQueue = VK_NULL_HANDLE;
    ASurfaceControl* mSurfaceControl = nullptr;
    int mWidth = 0;
    int mHeight = 0;

    BufferQueue mBufferQueue;
    GLuint mProgram = 0;
    GLuint mVao = 0;
    GLuint mVbo = 0;
    GLuint mEbo = 0;
};


#endif //HELLOSURFACECONTROL_CHILDSURFACE_H
