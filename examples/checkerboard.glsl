#version 450
#extension GL_EXT_shader_image_load_formatted : require
#extension GL_EXT_scalar_block_layout : require

layout(set = 0, binding = 0)
uniform image2D renderTarget;

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

void mainImage(out vec4 fragColor, vec2 fragCoord, vec2 iResolution)
{
    float cx = 2.0*fragCoord.x/iResolution.y-2.0;
    float cy = 2.0*fragCoord.y/iResolution.y-1.0;

    float zx = 0.0;
    float zy = 0.0;

    int iteration = 0;
    int max_iteration = 1000;

    // f(z) = z^2 + c, z and c are complex number.
    while ((zx*zx + zy*zy) <= 256.0 && iteration < max_iteration)
    {
        float next_zx = zx*zx - zy*zy + cx;
        zy = 2.0*zx*zy + cy;

        zx = next_zx;

        iteration++;
    }

    if (iteration == max_iteration)
        fragColor = vec4(0.0, 0.0, 0.0, 1.0);
    else
    {
        float smooth_iteration = float(iteration);

        // Smooth iteration use https://iquilezles.org/articles/msetsmooth/
        // |Zn| = zx*zx + zy*zy, B = 256.0, d = 2.0
        //smooth_iteration -= log(log(zx*zx + zy*zy)/log(256.0))/log(2.0);
        smooth_iteration -= log2(log2(zx*zx + zy*zy)) - 3.0; // Simplified with log2.

        fragColor = vec4(smooth_iteration/25.0, (smooth_iteration-25.0)/25.0, (smooth_iteration-50.0)/25.0, 1.0);
    }
}

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
    vec4 c = vec4(0.0, 0.0, 0.0, 1.0);
    vec2 point = vec2(gl_GlobalInvocationID.xy) / vec2(img_size);
    point = point * 2.0 - vec2(1.0);
    point = 4 * point;

    vec2 v0 = push_constants.triangle[0];
    vec2 v1 = push_constants.triangle[1];
    vec2 v2 = push_constants.triangle[2];

    float phi = push_constants.time;

    mat2 rot = mat2(vec2(cos(phi), -sin(phi)), vec2(sin(phi), cos(phi)));
    v0 *= rot;
    v1 *= rot;
    v2 *= rot;

    if (is_inside_edge(v0, v1, point))
        c.x = 1.0;
    if (is_inside_edge(v1, v2, point))
        c.y = 1.0;
    if (is_inside_edge(v2, v0, point))
        c.z = 1.0;
    
    //c.rgb = vec3(0, 0, 0);
    float r = length(v1 - v0) / 3;
    vec2 mid = (v0 + v1) / 2;
    vec2 huh = normalize(v2 - mid);
    float maxDickLength = (0.5 * sin(4 * phi) + 2) * length(v2 - mid);
    float magic = clamp(dot(point - mid, huh), 0.2, maxDickLength);
    vec2 pp = point - magic * huh + 0.1 * magic * magic * sin(17 * phi) * (v1 - v0);
    float dickr = 1 + 0.2 * sin(4 * phi);
    vec2 ballmove = -0.05 * sin(4 * phi) * (v1 - v0);
    if (
        dot(v0 - point + ballmove, v0 - point + ballmove) < r * r ||
        dot(v1 - point - ballmove, v1 - point - ballmove) < r * r ||
        dot(mid - pp, mid - pp) < r * r * dickr ||
        false
    ) {
        c.rgb = vec3(1, 1, 1);
    }
    r *= 0.9;
    if (
        dot(v0 - point + ballmove, v0 - point + ballmove) < r * r ||
        dot(v1 - point - ballmove, v1 - point - ballmove) < r * r ||
        dot(mid - pp, mid - pp) < r * r * dickr ||
        false
    ) {
        c.rgb = vec3(0, 0, 0);
        c.rgb = mix(vec3(240,184,160)/512, vec3(1,0,0), 0.6 * magic / maxDickLength);
    }

    //c.xy = push_constants.triangle[gl_GlobalInvocationID.x % 3];
    // switch(gl_GlobalInvocationID.x % 3) {
    //     case 0: c.xy = push_constants.triangle[0]; break;
    //     case 1: c.xy = push_constants.triangle[1]; break;
    //     case 2: c.xy = push_constants.triangle[2]; break;
    // }
    // c.xy = point;
    //c.z = 0.0;
    //vec4 c = (ok == 0) ? vec4(0.0) : vec4(1.0, 0.0, 1.0, 1.0);
    //mainImage(c, vec2(gl_GlobalInvocationID.xy), vec2(img_size));
    //if (ok == 0)
        imageStore(renderTarget, ivec2(gl_GlobalInvocationID.xy), c);
}