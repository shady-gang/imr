#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragUV;

layout(location = 0) out vec4 outColor;

#include "noise.glsl"

void main() {
    vec3 color = fragColor;

    float f = perlin_noise(fragUV);
    float off = 0.01;
    float fx = perlin_noise(fragUV + vec2(0.0, off));
    float fy = perlin_noise(fragUV + vec2(off, 0.0));
    float dx = (fx - f) / off;
    float dy = (fy - f) / off;
    vec3 normal = normalize(vec3(dx, dy, 1.0));
    //color = color * ((0.5 - 0.5 * f));
    //color = 0.5 + 0.5 * normal;
    vec3 light = normalize(vec3(0.5, 1.0, 0.25));
    vec3 diffuse = mix(vec3(0.1, 0.9, 0.3), vec3(1.0), clamp(pow(1.0 - f, 3), 0, 1.0));
    color = max(dot(light, normal), 0) * diffuse + vec3(0.2) * diffuse;
    //color = vec3(1.0);

    float depth = gl_FragCoord.z;
    vec3 fog = vec3(0.8f, 0.9f, 1.0f);
    //float fog_dropoff = clamp(pow(depth - 0.2, 6) + 0.4, 0, 1);
    float fog_dropoff = clamp(smoothstep(0.95, 1.0, depth), 0, 1);

    color = mix(color, fog, fog_dropoff);
    //color = vec3(fog_dropoff);

    outColor = vec4(color, 1.0);
}

/* vim: set filetype=cpp: */
