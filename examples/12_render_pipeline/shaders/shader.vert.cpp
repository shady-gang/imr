#include <stdint.h>
#include <shady.h>

#include "math.h"

using namespace vcc;
using namespace nasl;

location(0) input vec3 inPosition;
location(1) input vec3 inColor;

push_constant struct constants {
    mat4 render_matrix;
} PushConstants;

location(0) output vec3 fragColor;
location(1) output vec2 fragUV;

extern "C" {

vertex_shader void main() {
    vec4 input_position = (vec4){inPosition.x, inPosition.y, inPosition.z, 1.0};

    gl_Position = PushConstants.render_matrix * input_position;
    //gl_Position = test * input_position;

    fragColor = (vec3) {fragColor.x, fragColor.y, fragColor.z};
}

}

/*#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;

layout(push_constant) uniform constants {
    mat4 render_matrix;
} PushConstants;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragUV;

#include "noise.glsl"

void main() {
    vec4 input_position = vec4(inPosition, 1.0);
    vec2 uv = input_position.xz;

    float f = perlin_noise(uv);

    input_position.y += 0.8 * f;

    gl_Position = PushConstants.render_matrix * input_position;
    fragUV = uv;
    fragColor = inColor;
}*/

/* vim: set filetype=cpp: */
