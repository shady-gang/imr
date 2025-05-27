#version 450
#extension GL_EXT_shader_image_load_formatted : require
#extension GL_EXT_scalar_block_layout : require

layout(set = 0, binding = 0)
uniform image2D renderTarget;

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

void main() {
    ivec2 img_size = imageSize(renderTarget);
    if (gl_GlobalInvocationID.x >= img_size.x || gl_GlobalInvocationID.y >= img_size.y)
    return;

    vec4 c = vec4(0.0);

    vec2 x = (gl_GlobalInvocationID.xy / 16) % 2;

    if (x.x == x.y)
        c = vec4(1.0, 0.0, 1.0, 1.0);

    imageStore(renderTarget, ivec2(gl_GlobalInvocationID.xy), c);
}