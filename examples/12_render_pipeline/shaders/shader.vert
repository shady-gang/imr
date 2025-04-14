#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;

layout(push_constant) uniform constants { //Why not 420? Or uniform gpu? What does any of this mean?
    mat4 render_matrix;
} PushConstants;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragUV;

#include "noise.glsl"

void main() {
    vec4 input_position = vec4(inPosition, 1.0);
    vec2 uv = input_position.xz;

    float f = perlin_noise(uv);
    input_position.y += 0.8f * f;

    gl_Position = PushConstants.render_matrix * input_position;
    fragUV = uv;
    fragColor = inColor;
}

/* vim: set filetype=cpp: */
