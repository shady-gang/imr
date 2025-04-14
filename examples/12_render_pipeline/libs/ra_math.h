#pragma once

#include <cmath>
#include <cstddef>

static float sign(float f) {
    return copysignf(1.0f, f);
}

#include "nasl.h"


using namespace nasl;

static inline float epsilon = 1e-4f;
// static inline float pi = 3.141592f;

/// @brief Barycentric interpolation ([0,0] returns a, [1,0] returns b, and
/// [0,1] returns c).
template <typename T>
T interpolateBarycentric(const vec2 &bary, const T &a, const T &b,
                                const T &c) {
    return a * (1 - bary.x - bary.y) + b * bary.x + c * bary.y;
}

inline float clampf(float v, float min, float max) {
    return fminf(max, fmaxf(v, min));
}

inline vec3 clamp(vec3 v, vec3 min, vec3 max) {
    v.x = fminf(max.x, fmaxf(v.x, min.x));
    v.y = fminf(max.y, fmaxf(v.y, min.y));
    v.z = fminf(max.z, fmaxf(v.z, min.z));
    return v;
}

inline float color_average(vec3 color) {
    return (color[0] + color[1] + color[2]) / 3;
}

inline float color_luminance(vec3 color) {
    return color[0] * 0.2126f + color[1] * 0.7152f + color[2] * 0.0722f;
}

