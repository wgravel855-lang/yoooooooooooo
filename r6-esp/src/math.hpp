#pragma once
#include <cmath>

struct Vec2 {
    float x = 0.f, y = 0.f;
};

struct Vec3 {
    float x = 0.f, y = 0.f, z = 0.f;

    Vec3 operator-(const Vec3& o) const { return { x - o.x, y - o.y, z - o.z }; }
    Vec3 operator+(const Vec3& o) const { return { x + o.x, y + o.y, z + o.z }; }
    float length() const { return std::sqrt(x * x + y * y + z * z); }
};

// Column-major 4x4 view-projection matrix as the game stores it in memory.
// Laid out as a flat float[16]; m[row][col] access helper below.
struct Matrix4x4 {
    float m[4][4] = {};

    const float* flat() const { return &m[0][0]; }
    float* flat() { return &m[0][0]; }
};

// World -> screen projection using the engine's view-projection matrix.
// Returns false when the point is behind the camera (w <= epsilon) so the
// caller skips drawing it.
inline bool WorldToScreen(const Vec3& world, const Matrix4x4& vm,
                          int screenW, int screenH, Vec2& out)
{
    const float* M = vm.flat();

    // clip-space coordinates
    float clipX = world.x * M[0] + world.y * M[4] + world.z * M[8]  + M[12];
    float clipY = world.x * M[1] + world.y * M[5] + world.z * M[9]  + M[13];
    float clipW = world.x * M[3] + world.y * M[7] + world.z * M[11] + M[15];

    if (clipW < 0.01f)
        return false;

    // perspective divide -> normalized device coordinates
    float invW = 1.0f / clipW;
    float ndcX = clipX * invW;
    float ndcY = clipY * invW;

    // NDC -> pixel coordinates
    out.x = (screenW * 0.5f) * (1.0f + ndcX);
    out.y = (screenH * 0.5f) * (1.0f - ndcY);
    return true;
}
