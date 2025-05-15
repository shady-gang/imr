#version 450

layout (vertices=3) out;

layout(location = 0) in vec3 fragColorIn[];
layout(location = 1) in vec2 fragUVIn[];

layout(push_constant) uniform constants {
    mat4 render_matrix;
} PushConstants;

layout(location = 0) out vec3 fragColorOut[];
layout(location = 1) out vec2 fragUVOut[];

//#include "noise.glsl"

void main() {
    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
    fragColorOut[gl_InvocationID] = fragColorIn[gl_InvocationID];
    fragUVOut[gl_InvocationID] = fragUVIn[gl_InvocationID];

    if (gl_InvocationID == 0) {
        vec4 pos = PushConstants.render_matrix * gl_in[gl_InvocationID].gl_Position;

        float camera_distance = sqrt(pos.x * pos.x + pos.y * pos.y + pos.z * pos.z);
        float l = 1 - camera_distance / 20;
        l = l + 0.1;
        l = clamp(l, 0, 1) * 50;
        if (l <= 5)
            l = 5;
        if (pos.z < -2)
            l = 0;

        //l = 6;

        gl_TessLevelOuter[0] = l;
        gl_TessLevelOuter[1] = l;
        gl_TessLevelOuter[2] = l;

        gl_TessLevelInner[0] = l;
    }
}

/* vim: set filetype=cpp: */
