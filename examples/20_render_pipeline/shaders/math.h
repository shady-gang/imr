static float M_PI = 3.14159265f;
static float fabs(float) __asm__("shady::pure_op::GLSL.std.450::4::Invocation");
static float sign(float) __asm__("shady::pure_op::GLSL.std.450::6::Invocation");
static float floorf(float) __asm__("shady::pure_op::GLSL.std.450::8::Invocation");
static float sinf(float) __asm__("shady::pure_op::GLSL.std.450::13::Invocation");
static float cosf(float) __asm__("shady::pure_op::GLSL.std.450::14::Invocation");
static float tanf(float) __asm__("shady::pure_op::GLSL.std.450::15::Invocation");
static float powf(float, float) __asm__("shady::pure_op::GLSL.std.450::26::Invocation");
static float expf(float) __asm__("shady::pure_op::GLSL.std.450::27::Invocation");
static float logf(float) __asm__("shady::pure_op::GLSL.std.450::28::Invocation");
static float exp2f(float) __asm__("shady::pure_op::GLSL.std.450::29::Invocation");
static float log2f(float) __asm__("shady::pure_op::GLSL.std.450::30::Invocation");
//static float sqrtf(float) __asm__("shady::pure_op::GLSL.std.450::31::Invocation");
using vcc::sqrtf;
static float fminf(float, float) __asm__("shady::pure_op::GLSL.std.450::37::Invocation");
static float fmaxf(float, float) __asm__("shady::pure_op::GLSL.std.450::40::Invocation");
static float fmaf(float, float, float) __asm__("shady::pure_op::GLSL.std.450::50::Invocation");

typedef long int size_t;

#include <nasl.h>
#include <nasl_mat.h>

nasl::vec3 max(nasl::vec3 in1, nasl::vec3 in2) {
    return nasl::vec3 (
            fmaxf(in1.x, in2.x),
            fmaxf(in1.y, in2.y),
            fmaxf(in1.z, in2.z)
            );
}

float clamp(float in, float low, float hi) {
    if (in < low)
        return low;
    if (in > hi)
        return hi;
    return in;
}

float dot(nasl::vec2 a, nasl::vec2 b) {
    return a.x * b.x + a.y * b.y;
}

nasl::vec3 mix(nasl::vec3 x, nasl::vec3 y, float a) {
    return x * (1 - a) + y * a;
}

nasl::vec2 floor (nasl::vec2 p) {
    return nasl::vec2(floorf(p.x), floorf(p.y));
}

float step (float edge, float x) {
    if (x < edge)
        return 0.0;
    else
        return 1.0;
}

float smoothstep(float edge0, float edge1, float x) {
    float t = clamp((x - edge0) / (edge1 - edge0), 0.0, 1.0);
    return t * t * (3.0f - 2.0f * t);
}
