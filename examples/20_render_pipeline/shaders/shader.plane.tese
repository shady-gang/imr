#version 450
#extension GL_EXT_scalar_block_layout : enable

//layout (triangles, equal_spacing, cw) in;
layout (triangles, fractional_odd_spacing, cw) in;

layout(location = 0) in vec3 fragColorIn[];

layout(push_constant, scalar) uniform constants {
    mat4 render_matrix;
} PushConstants;

layout(location = 0) out vec3 fragColorOut;
layout(location = 1) out vec2 fragUVOut;

#include "noise.glsl"

void main()
{
    float u = gl_TessCoord.x;
    float v = gl_TessCoord.y;
    float w = gl_TessCoord.z;

    gl_Position = gl_in[0].gl_Position * u + gl_in[1].gl_Position * v + gl_in[2].gl_Position * w;
    fragColorOut = fragColorIn[0] * u + fragColorIn[1] * v + fragColorIn[2] * w;
    
    vec2 uv = gl_Position.xz;
    float f = perlin_noise(uv);
    gl_Position.y += 0.8f * f;
    gl_Position = PushConstants.render_matrix * gl_Position;
    
    fragUVOut = uv;
}

/* vim: set filetype=cpp: */
