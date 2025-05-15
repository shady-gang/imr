#version 450
#extension GL_EXT_shader_image_load_formatted : require
#extension GL_EXT_scalar_block_layout : require

layout(set = 0, binding = 0)
uniform image2D renderTarget;

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

layout(scalar, push_constant) uniform T {
	vec2 triangle[3];
	float time;
} push_constants;

bool is_inside_edge(vec2 e0, vec2 e1, vec2 p) {
    if (e1.x == e0.x)
      return (e1.x > p.x) ^^ (e0.y > e1.y);
    float a = (e1.y - e0.y) / (e1.x - e0.x);
    float b = e0.y + (0 - e0.x) * a;
    float ey = a * p.x + b;
    return (ey < p.y) ^^ (e0.x > e1.x);
}

void main() {
    ivec2 img_size = imageSize(renderTarget);
    if (gl_GlobalInvocationID.x >= img_size.x || gl_GlobalInvocationID.y >= img_size.y)
        return;

    uint ok = (gl_GlobalInvocationID.x + gl_GlobalInvocationID.y) % 2;
    vec4 c = vec4(1.0, 0.0, 0.2, 1.0);
    vec2 point = vec2(gl_GlobalInvocationID.xy) / vec2(img_size);
    point = point * 2.0 - vec2(1.0);

    vec2 v0 = push_constants.triangle[0];
    vec2 v1 = push_constants.triangle[1];
    vec2 v2 = push_constants.triangle[2];

    float phi = push_constants.time;

    if (!is_inside_edge(v0, v1, point))
        return;
    if (!is_inside_edge(v1, v2, point))
        return;
    if (!is_inside_edge(v2, v0, point))
        return;

    imageStore(renderTarget, ivec2(gl_GlobalInvocationID.xy), c);
}