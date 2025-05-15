#version 450

layout(location = 0) in vec3 inPosition;
//layout(location = 1) in vec3 inNormal;
layout(location = 1) in vec3 inColor;

layout(location = 0) out vec3 fragColor;

#include "noise.glsl"

void main() {
    vec4 input_position = vec4(inPosition, 1.0);

    gl_Position = input_position;
    fragColor = inColor;
}

/* vim: set filetype=cpp: */
