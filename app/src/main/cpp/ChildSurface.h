//
// Created by huang on 2025-01-20.
//

#ifndef HELLOSURFACECONTROL_CHILDSURFACE_H
#define HELLOSURFACECONTROL_CHILDSURFACE_H

#include <android/surface_control.h>
#include <GLES3/gl3.h>
#include <cstring>
#include <deque>
#include <bitset>

#include "BufferQueue.h"

class ChildSurface {
public:
    ChildSurface(VkDevice device, VkQueue queue);
    ~ChildSurface();

    bool init(ASurfaceControl* parent, const char* debugName);
    void resize(int width, int height);
    void draw();

    [[nodiscard]] int getWidth() const {
        return mWidth;
    }

    [[nodiscard]] int getHeight() const {
        return mHeight;
    }

    ASurfaceControl *getSurfaceControl() {
        return mSurfaceControl;
    }

    AHardwareBuffer* getCurrentBuffer() {
        return mBufferQueue.getCurrentBuffer();
    }

    void setCrop(const ARect& crop) {
        if (std::memcmp(&crop, &mCrop, sizeof(crop)) == 0) {
            return;
        }
        mChangedFlags[CROP_CHANGED] = true;
        mCrop = crop;
    }
    [[nodiscard]] const ARect& getCrop() const {
        return mCrop;
    }

    void setPosition(int left, int top) {
        if (mLeft == left && mTop == top) {
            return;
        }
        mChangedFlags[POSITION_CHANGED] = true;
        mLeft = left;
        mTop = top;
    }
    [[nodiscard]] int getLeft() const {
        return mLeft;
    }
    [[nodiscard]] int getTop() const {
        return mTop;
    }

    void setTransform(int transform) {
        if (mTransform == transform) {
            return;
        }
        mChangedFlags[TRANSFORM_CHANGED] = true;
        mTransform = transform;
    }
    [[nodiscard]] int getTransform() const {
        return mTransform;
    }

    void setScale(float xScale, float yScale) {
        if (mXScale == xScale && mYScale == yScale) {
            return;
        }
        mXScale = xScale;
        mYScale = yScale;
    }
    [[nodiscard]] float getXScale() const {
        return mXScale;
    }
    [[nodiscard]] float getYScale() const {
        return mYScale;
    }

    void setAlpha(float alpha) {
        if (mAlpha == alpha) {
            return;
        }
        mChangedFlags[ALPHA_CHANGED] = true;
        mAlpha = alpha;
    }
    [[nodiscard]] float getAlpha() const {
        return mAlpha;
    }

    void applyChanges(ASurfaceTransaction* transaction);

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

    enum : int {
        BUFFER_CHANGED = 0,
        CROP_CHANGED,
        POSITION_CHANGED,
        TRANSFORM_CHANGED,
        SCALE_CHANGED,
        ALPHA_CHANGED,
        MAX_CHANGED_FLAGS = 5
    };

    std::bitset<MAX_CHANGED_FLAGS> mChangedFlags;

    ARect mCrop = {};
    float mXScale = 1.0f;
    float mYScale = 1.0f;

    int mTop = 0;
    int mLeft = 0;

    int mTransform = 0;
    float mAlpha = 1.0f;
};


#endif //HELLOSURFACECONTROL_CHILDSURFACE_H
