#version 450
#extension GL_EXT_shader_image_load_formatted : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_buffer_reference : require

layout(location = 0)
out vec3 color;

layout(scalar, buffer_reference) buffer VertexBuffer {
    vec3 vertices[36];
    vec3 vertexColors[36];
};

layout(scalar, buffer_reference) buffer MatrixBuffer {
    mat4 matrix;
};

layout(scalar, push_constant) uniform T {
	VertexBuffer vertex_buffer;
    MatrixBuffer matrix_buffer;
    //mat4 matrix;
    //float time;
    int i, j;
} push_constants;

void main() {
    mat4 matrix = push_constants.matrix_buffer.matrix;
    vec3 vertex = push_constants.vertex_buffer.vertices[gl_VertexIndex];
    vertex.x += push_constants.i * 2;
    vertex.z += push_constants.j * 2;
    vertex.z += gl_InstanceIndex * 2;
    //matrix[3][0] = push_constants.i * 2;
    //matrix[3][2] = push_constants.j * 2;
    gl_Position = matrix * vec4(vertex, 1.0);
    color = push_constants.vertex_buffer.vertexColors[gl_VertexIndex];
}