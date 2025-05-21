#version 450
#extension GL_EXT_scalar_block_layout : enable

layout (vertices=3) out;

layout(location = 0) in vec3 fragColorIn[];
//layout(location = 1) in vec2 fragUVIn[];

layout(push_constant, scalar) uniform constants {
    layout(offset=16*4) mat4 render_matrix;
    float tesselation_factor;
} PushConstants;

layout(location = 0) out vec3 fragColorOut[];
//layout(location = 1) out vec2 fragUVOut[];

//#include "noise.glsl"

void main() {
    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
    fragColorOut[gl_InvocationID] = fragColorIn[gl_InvocationID];
    //fragUVOut[gl_InvocationID] = fragUVIn[gl_InvocationID];

    if (gl_InvocationID == 0) {
        vec4 v_pos1 = PushConstants.render_matrix * gl_in[0].gl_Position;
        vec4 v_pos2 = PushConstants.render_matrix * gl_in[1].gl_Position;
        vec4 v_pos3 = PushConstants.render_matrix * gl_in[2].gl_Position;

        if (v_pos1.z < -1 && v_pos2.z < -1 && v_pos3.z < -1) {
            gl_TessLevelOuter[0] = 0;
            gl_TessLevelOuter[1] = 0;
            gl_TessLevelOuter[2] = 0;

            gl_TessLevelInner[0] = 0;
        } else {
            vec4 e_pos1 = (v_pos1 + v_pos2) / 2;
            vec4 e_pos2 = (v_pos2 + v_pos3) / 2;
            vec4 e_pos3 = (v_pos3 + v_pos1) / 2;

            float camera_distance_1 = sqrt(e_pos1.x * e_pos1.x + e_pos1.y * e_pos1.y + e_pos1.z * e_pos1.z);
            float camera_distance_2 = sqrt(e_pos2.x * e_pos2.x + e_pos2.y * e_pos2.y + e_pos2.z * e_pos2.z);
            float camera_distance_3 = sqrt(e_pos3.x * e_pos3.x + e_pos3.y * e_pos3.y + e_pos3.z * e_pos3.z);

            float l1 = 1 - camera_distance_1 / 20 + 0.1;
            float l2 = 1 - camera_distance_2 / 20 + 0.1;
            float l3 = 1 - camera_distance_3 / 20 + 0.1;

            l1 = clamp(l1, 0, 1) * PushConstants.tesselation_factor;
            l2 = clamp(l2, 0, 1) * PushConstants.tesselation_factor;
            l3 = clamp(l3, 0, 1) * PushConstants.tesselation_factor;

            if (l1 <= PushConstants.tesselation_factor / 10) l1 = PushConstants.tesselation_factor / 10;
            if (l2 <= PushConstants.tesselation_factor / 10) l2 = PushConstants.tesselation_factor / 10;
            if (l3 <= PushConstants.tesselation_factor / 10) l3 = PushConstants.tesselation_factor / 10;

            gl_TessLevelOuter[0] = l2;
            gl_TessLevelOuter[1] = l3;
            gl_TessLevelOuter[2] = l1;

            float l = max(l1, max(l2, l3));
            gl_TessLevelInner[0] = l;
        }
    }
}

/* vim: set filetype=cpp: */
