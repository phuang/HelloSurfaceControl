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

struct ASurfaceControlDeleter {
    void operator()(ASurfaceControl* surfaceControl) const {
        ASurfaceControl_release(surfaceControl);
    }
};

using UniqueASurfaceControl = std::unique_ptr<ASurfaceControl, ASurfaceControlDeleter>;

class ChildSurface : public std::enable_shared_from_this<ChildSurface> {
public:
    ChildSurface(VkDevice device, VkQueue queue);
    ~ChildSurface();

    bool init(ASurfaceControl* parent, const char* debugName);
    void resize(int width, int height);
    void draw();

    void setCrop(const ARect& crop) {
        if (std::memcmp(&crop, &mCrop, sizeof(crop)) == 0) {
            return;
        }
        mChangedFlags[CROP_CHANGED] = true;
        mCrop = crop;
    }

    void setPosition(int left, int top) {
        if (mLeft == left && mTop == top) {
            return;
        }
        mChangedFlags[POSITION_CHANGED] = true;
        mLeft = left;
        mTop = top;
    }

    void setTransform(int transform) {
        if (mTransform == transform) {
            return;
        }
        mChangedFlags[TRANSFORM_CHANGED] = true;
        mTransform = transform;
    }

    void setScale(float xScale, float yScale) {
        if (mXScale == xScale && mYScale == yScale) {
            return;
        }
        mChangedFlags[SCALE_CHANGED] = true;
        mXScale = xScale;
        mYScale = yScale;
    }

    void setAlpha(float alpha) {
        if (mAlpha == alpha) {
            return;
        }
        mChangedFlags[ALPHA_CHANGED] = true;
        mAlpha = alpha;
    }

    void setColor(float r, float g, float b, float a) {
        if (mColor[0] == r && mColor[1] == g && mColor[2] == b && mColor[3] == a) {
            return;
        }
        mColor[0] = r;
        mColor[1] = g;
        mColor[2] = b;
        mColor[3] = a;
        mChangedFlags[COLOR_CHANGED] = true;
    }

    void setTransparent(bool transparent) {
        if (mTransparent == transparent) {
            return;
        }
        mTransparent = transparent;
        mChangedFlags[TRANSPARENT_CHANGED] = true;
    }

    void setAnimationDelta(float delta) {
        mDelta = delta;
    }

    void applyChanges(ASurfaceTransaction* transaction);

private:
    void drawGL();
    void createProgram();
    void setupBuffers();
    void setupFramebuffer();

    static void bufferReleasedCallback(void* context, int fenceFd);
    void bufferReleased(int fenceFd);

    VkDevice mDevice = VK_NULL_HANDLE;
    VkQueue mQueue = VK_NULL_HANDLE;

    UniqueASurfaceControl mSurfaceControl;

    int mWidth = 0;
    int mHeight = 0;

    BufferQueue mBufferQueue;
    GLuint mProgram = 0;
    GLuint mVao = 0;
    GLuint mVbo = 0;
    GLuint mEbo = 0;

    GLuint mFbo = 0;
    GLuint mRbo = 0;

    enum : int {
        VISIBILITY_CHANGED,
        CROP_CHANGED,
        POSITION_CHANGED,
        TRANSFORM_CHANGED,
        SCALE_CHANGED,
        ALPHA_CHANGED,
        COLOR_CHANGED,
        TRANSPARENT_CHANGED,
        MAX_CHANGED_FLAGS,
    };

    std::bitset<MAX_CHANGED_FLAGS> mChangedFlags;

    bool mVisible = true;
    ARect mCrop = {};
    float mXScale = 1.0f;
    float mYScale = 1.0f;

    int mTop = 0;
    int mLeft = 0;

    int mTransform = 0;
    float mAlpha = 1.0f;
    float mColor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    bool mTransparent = false;

    float mDelta = 1.0f;
};


#endif //HELLOSURFACECONTROL_CHILDSURFACE_H
