//
// Created by huang on 2025-01-20.
//

#include "ChildSurface.h"

#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <dlfcn.h>
#include <unistd.h>

#include "GLFence.h"
#include "Log.h"
#include "Matrix.h"

#define LOG_TAG "SurfaceControlApp"

static void *LibAndroid = nullptr;
using PFN_OnBufferRelease = void (*)(void *_Null_unspecified context,
                                     int release_fence_fd);
using PFN_ASurfaceTransaction_setBufferWithRelease = void (*)(
        ASurfaceTransaction *_Nonnull transaction,
        ASurfaceControl *_Nonnull surface_control,
        AHardwareBuffer *_Nonnull buffer,
        int acquire_fence_fd, void *_Null_unspecified context,
        PFN_OnBufferRelease _Nonnull func);
static PFN_ASurfaceTransaction_setBufferWithRelease pASurfaceTransaction_setBufferWithRelease = nullptr;

static std::chrono::system_clock::time_point kStartTime = std::chrono::system_clock::now();

ChildSurface::ChildSurface(VkDevice device, VkQueue queue) :
        mDevice(device), mQueue(queue), mBufferQueue(device) {
    if (!LibAndroid) {
        LibAndroid = dlopen("libandroid.so", RTLD_NOW);
        pASurfaceTransaction_setBufferWithRelease = reinterpret_cast<PFN_ASurfaceTransaction_setBufferWithRelease>(dlsym(
                LibAndroid, "ASurfaceTransaction_setBufferWithRelease"));
        if (!pASurfaceTransaction_setBufferWithRelease) {
            LOGE("Failed to find ASurfaceTransaction_setBufferWithRelease which is available in Android SDK API level 36+");
        }
    }
}

ChildSurface::~ChildSurface() {
    mSurfaceControl = nullptr;
    // release gl objects
    glDeleteProgram(mProgram);
    glDeleteBuffers(1, &mVbo);
    glDeleteVertexArrays(1, &mVao);
    glDeleteFramebuffers(1, &mFbo);
    glDeleteRenderbuffers(1, &mRbo);
}

bool ChildSurface::init(ASurfaceControl *parent, const char *debugName) {
    assert(mSurfaceControl == nullptr);

    mSurfaceControl.reset(ASurfaceControl_create(parent, debugName));
    if (mSurfaceControl == nullptr) {
        LOGE("Failed to create ASurfaceControl");
        return false;
    }

    createProgram();
    setupBuffers();

    return true;
}

void ChildSurface::resize(int width, int height) {
    LOGD("ChildSurface::resize() width=%d, height=%d", width, height);

    if (mWidth == width && mHeight == height) {
        return;
    }

    mWidth = width;
    mHeight = height;

    mBufferQueue.resize(width, height);
    setupFramebuffer();
}

void ChildSurface::draw() {
    drawGL();
}


// Shader sources (simple shaders to draw a triangle)
const char *vertexShaderSource =
        R"(#version 300 es
    in vec4 aPosition;
    in vec4 aColor;
    out vec4 vColor;
    in vec2 aTexCoord;
    out vec2 vTexCoord;
    uniform mat4 uRotationMatrix;
    void main() {
        gl_Position = uRotationMatrix * aPosition;
        vColor = aColor;
        vTexCoord = aTexCoord;
    }
)";

const char *fragmentShaderSource =
        R"(#version 300 es
    precision mediump float;
    in vec4 vColor;
    in vec2 vTexCoord;
    out vec4 fragColor;
    void main() {
        fragColor = vColor;
    }
)";

GLuint createShader(GLenum type, const char *source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLint logLength;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
        char *log = new char[logLength];
        glGetShaderInfoLog(shader, logLength, nullptr, log);
        LOGE("Shader compile error: %s", log);
        delete[] log;
    }
    return shader;
}

void ChildSurface::createProgram() {
    GLuint vertexShader = createShader(GL_VERTEX_SHADER, vertexShaderSource);
    GLuint fragmentShader = createShader(GL_FRAGMENT_SHADER, fragmentShaderSource);

    mProgram = glCreateProgram();
    glAttachShader(mProgram, vertexShader);
    glAttachShader(mProgram, fragmentShader);
    glLinkProgram(mProgram);

    GLint success;
    glGetProgramiv(mProgram, GL_LINK_STATUS, &success);
    if (!success) {
        GLint logLength;
        glGetProgramiv(mProgram, GL_INFO_LOG_LENGTH, &logLength);
        char *log = new char[logLength];
        glGetProgramInfoLog(mProgram, logLength, nullptr, log);
        LOGE("Program link error: %s", log);
        delete[] log;
    }
}

void ChildSurface::setupBuffers() {
    // clang-format off
    GLfloat cubeVertexArray[] = {
            // float4 position, float4 color, float2 uv,
            1, -1, 1, 1, 1, 0, 1, 1, 0, 1,
            -1, -1, 1, 1, 0, 0, 1, 1, 1, 1,
            -1, -1, -1, 1, 0, 0, 0, 1, 1, 0,
            1, -1, -1, 1, 1, 0, 0, 1, 0, 0,
            1, -1, 1, 1, 1, 0, 1, 1, 0, 1,
            -1, -1, -1, 1, 0, 0, 0, 1, 1, 0,

            1, 1, 1, 1, 1, 1, 1, 1, 0, 1,
            1, -1, 1, 1, 1, 0, 1, 1, 1, 1,
            1, -1, -1, 1, 1, 0, 0, 1, 1, 0,
            1, 1, -1, 1, 1, 1, 0, 1, 0, 0,
            1, 1, 1, 1, 1, 1, 1, 1, 0, 1,
            1, -1, -1, 1, 1, 0, 0, 1, 1, 0,

            -1, 1, 1, 1, 0, 1, 1, 1, 0, 1,
            1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
            1, 1, -1, 1, 1, 1, 0, 1, 1, 0,
            -1, 1, -1, 1, 0, 1, 0, 1, 0, 0,
            -1, 1, 1, 1, 0, 1, 1, 1, 0, 1,
            1, 1, -1, 1, 1, 1, 0, 1, 1, 0,

            -1, -1, 1, 1, 0, 0, 1, 1, 0, 1,
            -1, 1, 1, 1, 0, 1, 1, 1, 1, 1,
            -1, 1, -1, 1, 0, 1, 0, 1, 1, 0,
            -1, -1, -1, 1, 0, 0, 0, 1, 0, 0,
            -1, -1, 1, 1, 0, 0, 1, 1, 0, 1,
            -1, 1, -1, 1, 0, 1, 0, 1, 1, 0,

            1, 1, 1, 1, 1, 1, 1, 1, 0, 1,
            -1, 1, 1, 1, 0, 1, 1, 1, 1, 1,
            -1, -1, 1, 1, 0, 0, 1, 1, 1, 0,
            -1, -1, 1, 1, 0, 0, 1, 1, 1, 0,
            1, -1, 1, 1, 1, 0, 1, 1, 0, 0,
            1, 1, 1, 1, 1, 1, 1, 1, 0, 1,

            1, -1, -1, 1, 1, 0, 0, 1, 0, 1,
            -1, -1, -1, 1, 0, 0, 0, 1, 1, 1,
            -1, 1, -1, 1, 0, 1, 0, 1, 1, 0,
            1, 1, -1, 1, 1, 1, 0, 1, 0, 0,
            1, -1, -1, 1, 1, 0, 0, 1, 0, 1,
            -1, 1, -1, 1, 0, 1, 0, 1, 1, 0,
    };

    GLuint cubeIndices[] = {
            0, 1, 2, 3, 4, 5,
            6, 7, 8, 9, 10, 11,
            12, 13, 14, 15, 16, 17,
            18, 19, 20, 21, 22, 23,
            24, 25, 26, 27, 28, 29,
            30, 31, 32, 33, 34, 35
    };

    // Create VAO, VBO, and EBO
    glGenVertexArrays(1, &mVao);
    glGenBuffers(1, &mVbo);
    glGenBuffers(1, &mEbo);

    glBindVertexArray(mVao);

    glBindBuffer(GL_ARRAY_BUFFER, mVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertexArray), cubeVertexArray, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 10 * sizeof(GLfloat), (GLvoid *) 0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 10 * sizeof(GLfloat),
                          (GLvoid *) (4 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 10 * sizeof(GLfloat),
                          (GLvoid *) (8 * sizeof(GLfloat)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mEbo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cubeIndices), cubeIndices, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void ChildSurface::setupFramebuffer() {
    if (mFbo != 0) {
        glDeleteFramebuffers(1, &mFbo);
        mFbo = 0;
    }
    if (mRbo != 0) {
        glDeleteRenderbuffers(1, &mRbo);
        mRbo = 0;
    }

    glGenFramebuffers(1, &mFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, mFbo);

    glGenRenderbuffers(1, &mRbo);
    glBindRenderbuffer(GL_RENDERBUFFER, mRbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, mWidth, mHeight);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, mRbo);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
}

int bindEGLImageAsTexture(EGLImage eglImage) {
    GLuint textureId;
    glGenTextures(1, &textureId);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, textureId);
    glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, eglImage);
    return textureId;
}


void ChildSurface::drawGL() {
    auto image = mBufferQueue.produceImage();
    if (image.buffer == nullptr) {
        return;
    }

    if (image.fence) {
        image.fence->wait();
    }

    GLuint texture = bindEGLImageAsTexture(image.eglImage);

    // Bind the framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, mFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);

    // Bind the renderbuffer
    glBindRenderbuffer(GL_RENDERBUFFER, mRbo);


    // Check if the framebuffer is complete
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        LOGE("Framebuffer is not complete");
    }

    // Set the viewport
    glViewport(0, 0, mWidth, mHeight);

    // Clear the screen to red
    glClearColor(0.0f, 0.0f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Use the shader program
    glUseProgram(mProgram);

    auto time = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now() - kStartTime).count();

    auto computeAngle = [time, this](int period) {
        period *= mDelta;
        return 2 * M_PI * (time % period) / period;
    };

    float angleX = computeAngle(3000);
    float angleY = computeAngle(2000);
    float angleZ = computeAngle(1000);


    // Rotate the triangle by angle{X,Y,Z}
    Matrix4x4 rotationMatrix =
            Matrix4x4::Rotate(angleX, angleY, angleZ) * Matrix4x4::Scale(0.5f, 0.5f, 0.5f);

    GLint rotationMatrixLocation = glGetUniformLocation(mProgram, "uRotationMatrix");
    glUniformMatrix4fv(rotationMatrixLocation, 1, GL_FALSE, rotationMatrix.data);

    // set cull
    glEnable(GL_CULL_FACE);

    // Draw the cube
    glBindVertexArray(mVao);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mEbo);
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    mBufferQueue.enqueueProducedImage(GLFence::Create());
}

// static
void ChildSurface::bufferReleasedCallback(void *context, int fenceFd) {
    auto *weakSelf = reinterpret_cast<std::weak_ptr<ChildSurface> *>(context);
    if (auto self = weakSelf->lock()) {
        self->bufferReleased(fenceFd);
    } else {
        if (fenceFd > 0) {
            close(fenceFd);
        }
        LOGD("ChildSurface is already destroyed");
    }
    delete weakSelf;
}

void ChildSurface::bufferReleased(int fenceFd) {
    mBufferQueue.releasePresentImage(fenceFd);
}

void ChildSurface::applyChanges(ASurfaceTransaction *transaction) {
    auto image = mBufferQueue.presentImage();
    if (image.buffer != nullptr) {
        std::weak_ptr<ChildSurface> *weakSelf = new std::weak_ptr<ChildSurface>(shared_from_this());
        pASurfaceTransaction_setBufferWithRelease(transaction, mSurfaceControl.get(), image.buffer,
                                                  image.fence ? image.fence->getFd() : -1,
                                                  weakSelf, ChildSurface::bufferReleasedCallback);
    }
    if (mChangedFlags[CROP_CHANGED]) {
        ASurfaceTransaction_setCrop(transaction, mSurfaceControl.get(), mCrop);
    }
    if (mChangedFlags[POSITION_CHANGED]) {
        ASurfaceTransaction_setPosition(transaction, mSurfaceControl.get(), mLeft, mTop);
    }
    if (mChangedFlags[TRANSFORM_CHANGED]) {
        ASurfaceTransaction_setBufferTransform(transaction, mSurfaceControl.get(), mTransform);
    }
    if (mChangedFlags[SCALE_CHANGED]) {
        ASurfaceTransaction_setScale(transaction, mSurfaceControl.get(), mXScale, mYScale);
    }
    if (mChangedFlags[ALPHA_CHANGED]) {
        ASurfaceTransaction_setBufferAlpha(transaction, mSurfaceControl.get(), mAlpha);
    }
    if (mChangedFlags[COLOR_CHANGED]) {
        ASurfaceTransaction_setColor(transaction, mSurfaceControl.get(), mColor[0], mColor[1],
                                     mColor[2],
                                     mColor[3], ADataSpace::ADATASPACE_UNKNOWN);
    }
    if (mChangedFlags[TRANSPARENT_CHANGED]) {
        ASurfaceTransaction_setBufferTransparency(transaction, mSurfaceControl.get(), mTransparent
                                                                                      ? ASURFACE_TRANSACTION_TRANSPARENCY_TRANSPARENT
                                                                                      : ASURFACE_TRANSACTION_TRANSPARENCY_OPAQUE);
    }

    mChangedFlags.reset();
}
