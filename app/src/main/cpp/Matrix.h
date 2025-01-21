//
// Created by huang on 2025-01-21.
//

#ifndef HELLOSURFACECONTROL_MATRIX_H
#define HELLOSURFACECONTROL_MATRIX_H

#include <cmath>

struct Matrix4x4 {
    float data[16];

    static Matrix4x4 Identity() {
        Matrix4x4 matrix = {
                .data = {1.0f, 0.0f, 0.0f, 0.0f,
                         0.0f, 1.0f, 0.0f, 0.0f,
                         0.0f, 0.0f, 1.0f, 0.0f,
                         0.0f, 0.0f, 0.0f, 1.0f}

        };
        return matrix;
    }

    static Matrix4x4 RotateX(float angle) {
        float cosAngle = std::cos(angle);
        float sinAngle = std::sin(angle);

        Matrix4x4 matrix = {
                .data = {1.0f, 0.0f, 0.0f, 0.0f,
                         0.0f, cosAngle, -sinAngle, 0.0f,
                         0.0f, sinAngle, cosAngle, 0.0f,
                         0.0f, 0.0f, 0.0f, 1.0f}
        };
        return matrix;
    }

    static Matrix4x4 RotateY(float angle) {
        float cosAngle = std::cos(angle);
        float sinAngle = std::sin(angle);

        Matrix4x4 matrix = {
                .data = {cosAngle, 0.0f, sinAngle, 0.0f,
                         0.0f, 1.0f, 0.0f, 0.0f,
                         -sinAngle, 0.0f, cosAngle, 0.0f,
                         0.0f, 0.0f, 0.0f, 1.0f}
        };
        return matrix;
    }

    static Matrix4x4 RotateZ(float angle) {
        float cosAngle = std::cos(angle);
        float sinAngle = std::sin(angle);

        Matrix4x4 matrix = {
                .data = {cosAngle, -sinAngle, 0.0f, 0.0f,
                         sinAngle, cosAngle, 0.0f, 0.0f,
                         0.0f, 0.0f, 1.0f, 0.0f,
                         0.0f, 0.0f, 0.0f, 1.0f}
        };
        return matrix;
    }

    static Matrix4x4 Rotate(float angleX, float angleY, float angleZ) {
        return RotateX(angleX) * RotateY(angleY) * RotateZ(angleZ);
    }

    static Matrix4x4 Translate(float x, float y, float z) {
        Matrix4x4 matrix = {
                .data = {1.0f, 0.0f, 0.0f, x,
                         0.0f, 1.0f, 0.0f, y,
                         0.0f, 0.0f, 1.0f, z,
                         0.0f, 0.0f, 0.0f, 1.0f}
        };
        return matrix;
    }

    static Matrix4x4 Scale(float x, float y, float z) {
        Matrix4x4 matrix = {
                .data = {x, 0.0f, 0.0f, 0.0f,
                         0.0f, y, 0.0f, 0.0f,
                         0.0f, 0.0f, z, 0.0f,
                         0.0f, 0.0f, 0.0f, 1.0f}
        };
        return matrix;
    }

    Matrix4x4 operator*(const Matrix4x4& other) const {
        Matrix4x4 result = {};
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                result.data[i * 4 + j] = 0;
                for (int k = 0; k < 4; k++) {
                    result.data[i * 4 + j] += data[i * 4 + k] * other.data[k * 4 + j];
                }
            }
        }
        return result;
    }

    Matrix4x4& operator*=(const Matrix4x4& other) {
        *this = *this * other;
        return *this;
    }
};


#endif //HELLOSURFACECONTROL_MATRIX_H
