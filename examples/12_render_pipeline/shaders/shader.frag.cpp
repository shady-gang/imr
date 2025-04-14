#include <stdint.h>
#include <shady.h>

using namespace vcc;

location(0) input native_vec3 fragColor;
location(1) input native_vec2 fragUV;

location(0) output native_vec4 outColor;

extern "C" {

fragment_shader void main() {
    outColor = (native_vec4) {fragColor.x, fragColor.y, fragColor.z, 1.0f};
}

}



/*#version 450

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
    outColor = vec4(color, 1.0);
}*/

/* vim: set filetype=cpp: */
