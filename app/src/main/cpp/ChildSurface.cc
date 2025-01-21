//
// Created by huang on 2025-01-20.
//

#include "ChildSurface.h"

#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include "Log.h"

#define LOG_TAG "SurfaceControlApp"

ChildSurface::ChildSurface(VkDevice device, VkQueue queue) :
        mDevice(device), mQueue(queue), mBufferQueue(device) {}

ChildSurface::~ChildSurface() {
    if (mSurfaceControl) {
        ASurfaceControl_release(mSurfaceControl);
    }
}

bool ChildSurface::init(ASurfaceControl *parent, const char *debugName) {
    assert(mSurfaceControl == nullptr);

    mSurfaceControl = ASurfaceControl_create(parent, debugName);
    if (mSurfaceControl == nullptr) {
        LOGE("Failed to create ASurfaceControl");
        return false;
    }

    createProgram();
    setupBuffers();

    return mSurfaceControl != nullptr;
}

void ChildSurface::resize(int width, int height) {
    LOGD("ChildSurface::resize() width=%d, height=%d", width, height);

    if (mWidth == width && mHeight == height) {
        return;
    }

    mWidth = width;
    mHeight = height;

    mBufferQueue.resize(width, height);
}

void ChildSurface::draw() {
    drawGL();
}


// Shader sources (simple shaders to draw a triangle)
const char* vertexShaderSource = R"(#version 300 es
    in vec4 aPosition;
    in vec4 aColor;
    out vec4 vColor;
    void main() {
        gl_Position = aPosition;
        vColor = aColor;
    }
)";

const char* fragmentShaderSource = R"(#version 300 es
    precision mediump float;
    in vec4 vColor;
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
        char* log = new char[logLength];
        glGetProgramInfoLog(mProgram, logLength, nullptr, log);
        LOGE("Program link error: %s", log);
        delete[] log;
    }
}

void ChildSurface::setupBuffers() {
    // Vertex data for a triangle
    GLfloat vertices[] = {
            // Positions     // Colors
            0.0f,  0.5f,     1.0, 0.0f, 0.0f,  // Top vertex
            -0.5f, -0.5f,    0.0f, 1.0f, 0.0f,  // Bottom left vertex
            0.5f, -0.5f,    0.0f, 0.0f, 1.0f   // Bottom right vertex
    };

    GLuint indices[] = {  // Indices for triangle
            0, 1, 2
    };

    // Create VAO, VBO, and EBO
    glGenVertexArrays(1, &mVao);
    glGenBuffers(1, &mVbo);
    glGenBuffers(1, &mEbo);

    glBindVertexArray(mVao);

    glBindBuffer(GL_ARRAY_BUFFER, mVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mEbo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (GLvoid*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (GLvoid*)(2 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

int bindEGLImageAsTexture(EGLImage eglImage) {
    GLuint textureId;
    glGenTextures(1, &textureId);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, textureId);
    glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, eglImage);
    return textureId;
}


void ChildSurface::drawGL() {
    mBufferQueue.produceImage();
    auto eglImage = mBufferQueue.getCurrentProduceImage().eglImage;
    GLuint texture = bindEGLImageAsTexture(eglImage);

    // Create a framebuffer object
    GLuint fbo;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);

    // create a render buffer
    GLuint rbo;
    glGenRenderbuffers(1, &rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, mWidth, mHeight);

    // Attach the render buffer to the framebuffer
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);

    // Check if the framebuffer is complete
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        LOGE("Framebuffer is not complete");
    }

    // Set the viewport
    glViewport(0, 0, mWidth, mHeight);

    // Clear the screen to red
    glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Use the shader program
    glUseProgram(mProgram);

    // Draw the triangle
    glBindVertexArray(mVao);
    glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    // Unbind the framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);
    glDeleteRenderbuffers(1, &rbo);
    glDeleteTextures(1, &texture);

    mBufferQueue.enqueueProducedImage();
}