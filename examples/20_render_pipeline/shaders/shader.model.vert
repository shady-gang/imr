#version 450
#extension GL_EXT_scalar_block_layout : enable

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;

layout(push_constant, scalar) uniform constants {
    mat4 render_matrix;
    mat4 object_matrix;
} PushConstants;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragNormal;

mat4 translationMatrix(vec3 delta)
{
    return mat4(
        vec4(1.0, 0.0, 0.0, 0.0),
        vec4(0.0, 1.0, 0.0, 0.0),
        vec4(0.0, 0.0, 1.0, 0.0),
        vec4(delta, 1.0));
}

mat4 rotationMatrix(vec3 axis, float angle) {
    axis = normalize(axis);
    float s = sin(angle);
    float c = cos(angle);
    float oc = 1.0 - c;
    
    return mat4(oc * axis.x * axis.x + c,           oc * axis.x * axis.y - axis.z * s,  oc * axis.z * axis.x + axis.y * s,  0.0,
                oc * axis.x * axis.y + axis.z * s,  oc * axis.y * axis.y + c,           oc * axis.y * axis.z - axis.x * s,  0.0,
                oc * axis.z * axis.x - axis.y * s,  oc * axis.y * axis.z + axis.x * s,  oc * axis.z * axis.z + c,           0.0,
                0.0,                                0.0,                                0.0,                                1.0);
}

void main() {
    vec4 input_position = vec4(inPosition, 1.0);
    vec4 input_normal = vec4(inNormal, 0.0);

    /*mat4 flip_matrix = mat4 (1,  0, 0, 0,
                             0, -1, 0, 0,
                             0,  0, 1, 0,
                             0,  0, 0, 1);

    mat4 translate_matrix = translationMatrix(vec3(-10, 1, 13.5));
    mat4 rotation_matrix = rotationMatrix(vec3(0, 1, 0), radians(180));

    mat4 transform_matrix = PushConstants.render_matrix * translate_matrix * rotation_matrix * flip_matrix;

    vec4 output_position = transform_matrix * input_position;*/
    //vec4 output_normal = transform_matrix * input_normal;

    vec4 output_position = PushConstants.render_matrix * PushConstants.object_matrix * input_position;
    //vec4 output_normal = PushConstants.render_matrix * PushConstants.object_matrix * input_normal;
    vec4 output_normal = input_normal;

    gl_Position = output_position;
    fragNormal = output_normal.xyz;
    fragColor = inColor;
    //fragColor = vec3(0.32, 0.25, 0.16);
}

/* vim: set filetype=cpp: */
