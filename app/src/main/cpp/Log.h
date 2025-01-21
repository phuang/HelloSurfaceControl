//
// Created by huang on 2025-01-20.
//

#ifndef HELLOSURFACECONTROL_LOG_H
#define HELLOSURFACECONTROL_LOG_H

#include <android/log.h>
#include <cassert>

#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

#endif //HELLOSURFACECONTROL_LOG_H
