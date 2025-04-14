#include <stdint.h>
#include <shady.h>

#include "math.h"

using namespace vcc;
using namespace nasl;

#include "noise.h"

location(0) input native_vec3 inPosition;
location(1) input native_vec3 inColor;

push_constant struct constants1 {
    mat4 render_matrix;
} PushConstants;

location(0) output native_vec3 fragColor;
location(1) output native_vec2 fragUV;

extern "C" {

vertex_shader void main() {
    vec4 input_position = vec4(inPosition, 1.0);
    vec2 uv = inPosition.xz;

    float f = perlin_noise(uv);
    input_position.y = input_position.y + 0.8f * f;

    gl_Position = PushConstants.render_matrix * input_position;
    fragUV = uv;
    fragColor = inColor;
}

}

/* vim: set filetype=cpp: */
