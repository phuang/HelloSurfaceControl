//
// Created by huang on 2025-01-23.
//

#ifndef HELLOSURFACECONTROL_SCOPEDFD_H
#define HELLOSURFACECONTROL_SCOPEDFD_H

#include <unistd.h>

class ScopedFd {
public:
    ScopedFd() = default;
    explicit ScopedFd(int fd) : mFd(fd) {}
    ~ScopedFd() {
        reset();
    }

    ScopedFd(ScopedFd&& other) noexcept {
        mFd = other.mFd;
        other.mFd = -1;
    }

    ScopedFd& operator=(ScopedFd&& other) noexcept {

        if (this != &other) {
            reset();
            mFd = other.mFd;
            other.mFd = -1;
        }
        return *this;
    }

    bool isValid() const { return mFd != -1; }

    void reset(int fd) {
        if (fd < 0) {
            fd = -1;
        }
        if (mFd != -1) {
            close(mFd);
        }
        mFd = fd;
    }

    void reset() {
        reset(-1);
    }

    int get() const { return mFd; }

    int release() {
        int fd = mFd;
        mFd = -1;
        return fd;
    }

private:
    int mFd = -1;
};

#endif //HELLOSURFACECONTROL_SCOPEDFD_H
