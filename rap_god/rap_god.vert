#version 450
#extension GL_EXT_shader_image_load_formatted : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_buffer_reference : require

layout(location = 0)
in vec3 vertexIn;

layout(location = 1)
in vec3 colorIn;

layout(location = 2)
in vec2 uvIn;

layout(location = 0)
out vec3 colorOut;

layout(location = 1)
out vec2 uvOut;

layout(scalar, push_constant) uniform T {
    mat4 matrix;
    float time;
} push_constants;

void main() {
    mat4 matrix = push_constants.matrix;
    gl_Position = matrix * vec4(vertexIn, 1.0);
    uvOut = uvIn;
    colorOut = colorIn;
}