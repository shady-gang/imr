#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;

layout(push_constant) uniform constants { //Why not 420? Or uniform gpu? What does any of this mean?
    mat4 render_matrix;
} PushConstants;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragNormal;

//#include "noise.glsl"

void main() {
    vec4 input_position = vec4(inPosition, 1.0);
    vec4 input_normal = vec4(inPosition, 0.0);

    //float f = perlin_noise(uv);
    //input_position.y += 0.8f * f;

    gl_Position = PushConstants.render_matrix * input_position;
    //vec4 transformed_normal = PushConstants.render_matrix * input_normal;

    //fragNormal = transformed_normal.xyz;
    fragNormal = inNormal;
    fragColor = inColor;
}

/* vim: set filetype=cpp: */
