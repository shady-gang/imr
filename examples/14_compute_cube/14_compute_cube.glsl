#version 450
#extension GL_EXT_shader_image_load_formatted : require
#extension GL_EXT_scalar_block_layout : require

layout(set = 0, binding = 0)
uniform image2D renderTarget;

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

struct Tri { vec3 v0, v1, v2; vec3 color; };

layout(scalar, push_constant) uniform T {
	Tri triangle;
	float time;
} push_constants;

float cross2d(vec2 a, vec2 b) {
    return a.x * b.y - a.y * b.x;
}

float barCoord(const vec2 a, const vec2 b, const vec2 point) {
    return (a.x - point.x) * (b.y - a.y) - (a.y - point.y) * (b.x - a.x);
}

void main() {
    ivec2 img_size = imageSize(renderTarget);
    if (gl_GlobalInvocationID.x >= img_size.x || gl_GlobalInvocationID.y >= img_size.y)
        return;

    vec2 point = vec2(gl_GlobalInvocationID.xy) / vec2(img_size);
    point = point * 2.0 - vec2(1.0);

    vec2 v0 = push_constants.triangle.v0.xy;
    vec2 v1 = push_constants.triangle.v1.xy;
    vec2 v2 = push_constants.triangle.v2.xy;

    float scaling = barCoord(v0, v1, v2);

    float u = barCoord(v0, v1, point) / scaling;
    float v = barCoord(v1, v2, point) / scaling;

    if (u < 0.0 || u > 1.0)
        return;
    if (v < 0.0 || u + v > 1.0)
        return;

    if (scaling < 0)
        return;

    imageStore(renderTarget, ivec2(gl_GlobalInvocationID.xy), vec4(push_constants.triangle.color, 1));
    //imageStore(renderTarget, ivec2(gl_GlobalInvocationID.xy), vec4(u, v, 1.0 - u - v, 1));
}