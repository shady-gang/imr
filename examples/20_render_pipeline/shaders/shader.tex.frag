#version 450
#extension GL_EXT_scalar_block_layout : enable

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragUV;

layout(push_constant, scalar) uniform constants {
    layout (offset = 32 * 4) int fog_power;
    float fog_dropoff_lower;
    float fog_dropoff_upper;
} PushConstants;

layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform sampler2D texSampler;

void main() {
    float top_region = -0.25; //higher value = more white tops.

    vec3 color = fragColor;

    vec3 normal = fragNormal;

    //color = color * ((0.5 - 0.5 * f));
    //color = 0.5 + 0.5 * normal;
    vec3 light = normalize(vec3(-0.5, -1.0, 0.25));


    //vec3 diffuse = mix(vec3(0.1, 0.9, 0.3), vec3(1.0), clamp(pow(top_region - f, 3), 0, 1.0));
    //vec3 diffuse = vec3(0.5, 0.5, 0.5);
    vec3 diffuse = color;
    //vec3 diffuse = fragColor;
    color = max(dot(light, normal), 0) * diffuse + vec3(0.2) * diffuse;
    //color = vec3(1.0);

    float depth = gl_FragCoord.z;

    vec3 fog = vec3(0.8f, 0.9f, 1.0f);

    //float fog_dropoff = clamp(pow(depth - 0.2, 6) + 0.4, 0, 1);
    //float fog_dropoff = clamp(smoothstep(0.95, 1.0, depth), 0, 1);
    float fog_dropoff = clamp(smoothstep(PushConstants.fog_dropoff_lower, PushConstants.fog_dropoff_upper, depth), 0, 1);
    if (PushConstants.fog_power > 1)
        fog_dropoff = pow(fog_dropoff, PushConstants.fog_power);
    if (PushConstants.fog_power < 0)
        fog_dropoff = pow(fog_dropoff, 1.0f / (- PushConstants.fog_power));

    color = mix(color, fog, fog_dropoff);
    //color = vec3(fog_dropoff);

    //vec3 normal = fragNormal;

    //vec3 light = normalize(vec3(0, 0, 1)); //front to back
    //vec3 light = normalize(vec3(0, 1, 0)); //top to bottom
    //vec3 light = normalize(vec3(1, 0, 0)); //right to left

    //vec3 light = normalize(vec3(-0.2, 0.4, 1)); //right to left

    //vec3 diffuse = color;
    //color = max(dot(light, normal), 0) * diffuse + vec3(0.2) * diffuse;

    //outColor = vec4(color, 1.0);
    outColor = texture(texSampler, fragUV);
}

/* vim: set filetype=cpp: */
