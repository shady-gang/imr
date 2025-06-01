#version 450
#extension GL_EXT_shader_image_load_formatted : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_debug_printf : enable

precise float;
precise vec2;
precise vec3;
precise vec4;

layout(set = 0, binding = 0)
uniform image2D renderTarget;

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

struct Tri { vec3 v0, v1, v2; vec3 color; };

layout(scalar, push_constant) uniform T {
	Tri triangle;
    mat4 m;
    vec3 forward;
    vec3 position;
	float time;
} push_constants;

float cross_2(vec2 a, vec2 b) {
    return cross(vec3(a, 0), vec3(b, 0)).z;
}

int cross_2(ivec2 a, ivec2 b) {
    return a.x * b.y - a.y * b.x;
}

float barCoord(vec2 a, vec2 b, vec2 point){
    precise vec2 PA = point - a;
    precise vec2 BA = b - a;
    return cross_2(PA, BA);
}

int barCoord(ivec2 a, ivec2 b, ivec2 point) {
    ivec2 PA = point - a;
    ivec2 BA = b - a;
    return cross_2(PA, BA);
}

vec3 barycentricTri2(vec2 v0, vec2 v1, vec2 v2, vec2 point) {
    precise float scale = barCoord(v0.xy, v1.xy, v2.xy);

    precise float u = barCoord(v0.xy, v1.xy, point) / scale;
    precise float v = barCoord(v1.xy, v2.xy, point) / scale;

    return vec3(u, v, scale);
}

bool is_inside_edge(vec2 e0, vec2 e1, vec2 p) {
    if (e1.x == e0.x)
        return (e1.x > p.x) ^^ (e0.y > e1.y);
    float a = (e1.y - e0.y) / (e1.x - e0.x);
    float b = e0.y + (0 - e0.x) * a;
    float ey = a * p.x + b;
    return (ey < p.y) ^^ (e0.x > e1.x);
}

vec3 barycentricTri2i(ivec2 v0, ivec2 v1, ivec2 v2, ivec2 point) {
    int scalei = barCoord(v0.xy, v1.xy, v2.xy);
    float scale = scalei;
    //float scale = barCoord(vec2(v0.xy), vec2(v1.xy), vec2(v2.xy));

    float ui = barCoord(v0.xy, v1.xy, point);
    float vi = barCoord(v1.xy, v2.xy, point);

    float u = ui;
    float v = vi;

    if (gl_GlobalInvocationID.xy == uvec2(512)) {
        debugPrintfEXT("transformed v0 (%d, %d) v1 (%d, %d) v2 (%d, %d) point (%d, %d)", v0.x, v0.y, v1.x, v1.y, v2.x, v2.y, point.x, point.y);
        debugPrintfEXT("scale %f u %d v %d u/s %f v/s %f ", scale, ui, vi, u / scale, v / scale);
    }

    return vec3(u / scale, v / scale, scale);
}

struct Segment {
    vec2 e0;
    vec2 e1;
};

Segment clip_segment_ndc(Segment seg) {
    vec2 e0 = seg.e0;
    vec2 e1 = seg.e1;

    return Segment(e0, e1);
}

vec4 clip_intersect_edge(vec4 v0, vec4 v1, float value0, float value1) {
    float t = value0 / (value0 - value1);

    return (1.f - t) * v0 + t * v1;
}

bool tri_intersect(vec3 origin, vec3 dir, vec3 v0, vec3 v1, vec3 v2, out float u, out float v, out float t) {
    vec3 edge1 = v1 - v0;
    vec3 edge2 = v2 - v0;

    vec3 pvec = cross(dir, edge2);
    float det  = dot(edge1, pvec);
    if (det > -1e-8f && det < 1e-8f)
        return false;
    float invDet = 1 / det;

    vec3 tvec = origin - v0;
    u = dot(tvec, pvec) * invDet;
    if (u < 0 || u > 1)
        return false;

    vec3 qvec = cross(tvec, edge1);
    v = dot(dir, qvec) * invDet;
    if (v < 0 || u + v > 1)
        return false;

    t = dot(edge2, qvec) * invDet;
    //if (t < epsilon)
    //    return false;

    //if (t < ray.tmin || t > ray.tmax)
    //    return false;

    return true;
}

void main() {
    ivec2 img_size = imageSize(renderTarget);
    if (gl_GlobalInvocationID.x >= img_size.x || gl_GlobalInvocationID.y >= img_size.y)
        return;

    vec2 ndc_ray = vec2(gl_GlobalInvocationID.xy) / vec2(img_size);
    ndc_ray = ndc_ray * 2.0 - vec2(1.0);

    ivec2 ss_fragCoord = ivec2(gl_GlobalInvocationID.xy);
    //ivec2 ss_fragCoord = ivec2(gl_GlobalInvocationID.xy) * 2 - img_size;

    // original, object-space coordinates of the triangle vertices
    vec4 os_v0 = vec4(push_constants.triangle.v0, 1);
    vec4 os_v1 = vec4(push_constants.triangle.v1, 1);
    vec4 os_v2 = vec4(push_constants.triangle.v2, 1);
    // transformed triangle vertices in clip space
    precise vec4 v0 = push_constants.m * os_v0;
    precise vec4 v1 = push_constants.m * os_v1;
    precise vec4 v2 = push_constants.m * os_v2;

    //vec4 equation = vec4(0, 0, 1, 1);
    //float values[3] = {
    //    dot(v0, equation),
    //    dot(v1, equation),
    //    dot(v2, equation),
    //};
    //uint mask = 0;
    //if (values[0] < 0)
    //    mask |= 1u;
    //if (values[1] < 0)
    //    mask |= 2u;
    //if (values[2] < 0)
    //    mask |= 4u;
    ////if (values[0] < 0) {
    ////    v0 = clip_intersect_edge(v2, v0, values[2], values[0]);
    ////}
    //switch (mask) {
    //    case 6:
    //        v0 = v0;
    //        v1 = clip_intersect_edge(v1, v0, values[1], values[0]);
    //        v2 = clip_intersect_edge(v2, v0, values[2], values[0]);
    //        break;
    //}
    //if (gl_GlobalInvocationID.xy == uvec2(0)) {
    //    debugPrintfEXT("values %f %f %f ", values[0], values[1], values[2]);
    //}

    if (v0.w < 0 && v1.w < 0 && v2.w < 0)
        return;

    mat4 im = inverse(push_constants.m);
    //vec4 wsRayOrigin = im * vec4(0, 0, 0, 1);
    //vec4 wsRayDir = vec4(0, 0, -1, 1) * im;
    //wsRayOrigin /= wsRayOrigin.w;
    //wsRayDir.xyz = normalize(wsRayDir.xyz);
    //wsRayDir.w = 0;
    vec3 pos = push_constants.position;
    vec3 dir = push_constants.forward;
    vec4 origin2 = push_constants.m * vec4(vec3(0, 1, 0), 1.0);
    origin2 /= origin2.w;
    //origin2.xy = vec2(0);
    if (distance(origin2.xy, ndc_ray.xy) < 0.01) {
        imageStore(renderTarget, ivec2(gl_GlobalInvocationID.xy), vec4(1.0, 0, 0, 1));
        //return;
    }
    origin2 = vec4(0, 0, -1, 1);
    vec4 dir2 = im * vec4(ndc_ray.x, ndc_ray.y, 1, 1);

    if (gl_GlobalInvocationID.xy == uvec2(0)) {
        //debugPrintfEXT("wsRayOrigin %f %f %f %f ", wsRayOrigin.x, wsRayOrigin.y, wsRayOrigin.z, wsRayOrigin.w);
        //debugPrintfEXT("wsRayDir %f %f %f %f ", wsRayDir.x, wsRayDir.y, wsRayDir.z, wsRayDir.w);
        //pos = -pos + dir * 0.1;
        debugPrintfEXT("pos %f %f %f ", pos.x, pos.y, pos.z);
        //debugPrintfEXT("dir %f %f %f ", dir.x, dir.y, dir.z);
        //debugPrintfEXT("length(dir) %f ", length(dir));
        //vec4 dir2 = push_constants.m * vec4(dir, 0);
        //dir2 /= dir2.w;
        //origin2 = vec4(0, 0, 3, 1);
        debugPrintfEXT("xformed %f %f %f %f ", origin2.x, origin2.y, origin2.z, origin2.w);
        //vec4 origin2 = push_constants.m * vec4(-pos + dir * 0.1, 1);
        //origin2 = push_constants.m[3];
        //origin2 = im[3];
        //origin2.xyz = vec3(push_constants.m[0][3], push_constants.m[1][3], push_constants.m[2][3]);
        debugPrintfEXT("dir2 %f %f %f %f ", dir2.x, dir2.y, dir2.z, dir2.w);
    }
    origin2 = im * vec4(origin2);
    origin2 /= origin2.w;
    if (gl_GlobalInvocationID.xy == uvec2(0)) {
        debugPrintfEXT("origin2 %f %f %f %f ", origin2.x, origin2.y, origin2.z, origin2.w);
        debugPrintfEXT("\n");
    }

    float u, v, tt;
    if (tri_intersect(origin2.xyz, normalize(dir2.xyz), os_v0.xyz, os_v1.xyz, os_v2.xyz, u, v, tt)) {
        imageStore(renderTarget, ivec2(gl_GlobalInvocationID.xy), vec4(1.0, 1, 0, 1));
    }

    vec3 ndc_v0 = v0.xyz / v0.w;
    vec3 ndc_v1 = v1.xyz / v1.w;
    vec3 ndc_v2 = v2.xyz / v2.w;

    vec3 ndc_normal = normalize(cross(ndc_v0 - ndc_v2, ndc_v2 - ndc_v1));
    float t = dot(ndc_normal, vec3(0, 0, -1));
    //if (gl_GlobalInvocationID.xy == uvec2(0)) {
    //    debugPrintfEXT("ndcNormal %f %f %f t %f ", ndc_normal.x, ndc_normal.y, ndc_normal.z, t);
    //}

    // screen-space coordinates of the triangles, plus normalized Z
    ivec2 ss_v0 = ivec2(((ndc_v0.xy * 0.5 + vec2(0.5)) * vec2(img_size)));
    ivec2 ss_v1 = ivec2(((ndc_v1.xy * 0.5 + vec2(0.5)) * vec2(img_size)));
    ivec2 ss_v2 = ivec2(((ndc_v2.xy * 0.5 + vec2(0.5)) * vec2(img_size)));

    ivec2 ss_v0_clipped = ss_v0;

    ivec2 e10 = ss_v1.xy - ss_v0_clipped.xy;
    ivec2 ep0 = ss_fragCoord - ss_v0_clipped.xy;

    int scale_10 = 10;

    //e10 /= scale_10;
    //ep0 *= scale_10;

    ivec2 e02 = ss_v0.xy - ss_v2.xy;
    ivec2 ep2 = ss_fragCoord - ss_v2.xy;

    ivec2 e21 = ss_v2.xy - ss_v1.xy;
    ivec2 ep1 = ss_fragCoord - ss_v1.xy;

    //if (gl_GlobalInvocationID.xy == uvec2(0)) {
    //    debugPrintfEXT("e10 %d %d e02 %d %d e21 %d %d ", e10.x, e10.y, e02.x, e02.y, e21.x, e21.y);
    //}

    //float scale = barCoord(vec2(ss_v0.xy), vec2(ss_v1.xy), vec2(ss_v2.xy));
    float scale = cross_2(e02, e10);

    float bary_v2 = float(cross_2(e10, ep0)) / scale;
    float bary_v1 = float(cross_2(e02, ep2)) / scale;
    float bary_v0 = float(cross_2(e21, ep1)) / scale;
    //if (gl_GlobalInvocationID.xy == uvec2(0)) {
    //    debugPrintfEXT("baryCentric %f %f %f ", bary_v0, bary_v1, bary_v2);
    //}
    //float bary_v2 = barCoord(ss_v0.xy, ss_v1.xy, ss_fragCoord) / scale;
    //float bary_v1 = barCoord(ss_v2.xy, ss_v0.xy, ss_fragCoord) / scale;
    //float bary_v0 = barCoord(ss_v1.xy, ss_v2.xy, ss_fragCoord) / scale;s

    //if (v2.w < 0)
    //    u = -u;
    if (v0.w < 0) {
        bary_v1 = bary_v1;
        bary_v2 = bary_v2;
        //bary_v0 = -bary_v0;
        //u = -u;
        //v = -v;
    }
    //if (v1.w < 0) {
    //    w = -w;
    //}
    bool checkerCoarse = ((gl_GlobalInvocationID.x / 2 + gl_GlobalInvocationID.y / 2) % 2 == 0);
    bool checkerFine = ((gl_GlobalInvocationID.x + gl_GlobalInvocationID.y) % 2 == 0);

    //if (checkerCoarse && ((bary_v0 < 0.0) || (bary_v1 < 0.0) || (bary_v2 < 0.0)))
    //    return;
    //precise float w = 1.0 - u - v;

    if (gl_GlobalInvocationID.xy == uvec2(0)) {
        //debugPrintfEXT("v0 %d %d ", ss_v0.x, ss_v0.y);
        //debugPrintfEXT("v1 %d %d ", ss_v2.x, ss_v1.y);
        //debugPrintfEXT("v2 %d %d ", ss_v1.x, ss_v2.y);
        //debugPrintfEXT("v0 %f %f %f %f ", v0.x, v0.y, v0.z, v0.w);
        //debugPrintfEXT("v1 %f %f %f %f ", v1.x, v1.y, v1.z, v1.w);
        //debugPrintfEXT("v2 %f %f %f %f ", v2.x, v2.y, v2.z, v2.w);
        //debugPrintfEXT("v0 %f %f %f %f ", os_v0.x, os_v0.y, os_v0.z, os_v0.w);
        //debugPrintfEXT("v1 %f %f %f %f ", os_v1.x, os_v1.y, os_v1.z, os_v1.w);
        //debugPrintfEXT("v2 %f %f %f %f ", os_v2.x, os_v2.y, os_v2.z, os_v2.w);
        //debugPrintfEXT("baryCentric %f %f %f ", v, w, u);
        //debugPrintfEXT("\n");
    }

    // for the purposes of visibility, we have to invert the test whenever a given vertex lies behind the camera
    //if ((u < 0.0 ^^ v2.w < 0) || (v < 0.0 ^^ v0.w < 0) || (w < 0.0 ^^ v1.w < 0))
    //    return;

    // we need to invert the front/backface culling once for each vertex behind the camera
    //if (scale > 0 ^^ v0.w < 0 ^^ v1.w < 0 ^^ v2.w < 0)
    //    return;
    //bool backface = (is_inside_edge(ss_v1.xy, ss_v0.xy, ss_fragCoord) ^^ (v0.w < 0) ^^ (v1.w < 0)) && (is_inside_edge(ss_v2.xy, ss_v1.xy, ss_fragCoord) ^^ (v1.w < 0) ^^ (v2.w < 0)) && (is_inside_edge(ss_v0.xy, ss_v2.xy, ss_fragCoord) ^^ (v2.w < 0) ^^ (v0.w < 0));
    bool backface = checkerFine && (is_inside_edge(ss_v1.xy, ss_v0.xy, ss_fragCoord)) && (is_inside_edge(ss_v2.xy, ss_v1.xy, ss_fragCoord)) && (is_inside_edge(ss_v0.xy, ss_v2.xy, ss_fragCoord));

    bool frontface = (is_inside_edge(ss_v0.xy, ss_v1.xy, ss_fragCoord) ^^ (v0.w < 0) ^^ (v1.w < 0)) && (is_inside_edge(ss_v1.xy, ss_v2.xy, ss_fragCoord) ^^ (v1.w < 0) ^^ (v2.w < 0)) && (is_inside_edge(ss_v2.xy, ss_v0.xy, ss_fragCoord) ^^ (v2.w < 0) ^^ (v0.w < 0));
    //bool frontface = (is_inside_edge(ss_v0.xy, ss_v1.xy, ss_fragCoord)) && (is_inside_edge(ss_v1.xy, ss_v2.xy, ss_fragCoord)) && (is_inside_edge(ss_v2.xy, ss_v0.xy, ss_fragCoord));
    if (!frontface)
        return;

    vec4 c = vec4(push_constants.triangle.color, 1);

    vec3 v_ws = vec3(v0.w, v1.w, v2.w);
    vec3 ss_v_coefs = vec3(bary_v0, bary_v1, bary_v2);
    vec3 pc_v_coefs = ss_v_coefs / v_ws;
    float pc_v_coefs_sum = pc_v_coefs.x + pc_v_coefs.y + pc_v_coefs.z;
    pc_v_coefs = pc_v_coefs / pc_v_coefs_sum;

    //float depth = dot(pc_v_coefs, vec3(v0.z, v1.z, v2.z));
    float depth = dot(pc_v_coefs, vec3(v0.w, v1.w, v2.w));
    //float depth = dot(pc_v_coefs, vec3(ss_v0.z, ss_v1.z, ss_v2.z));

    //c.xyz = vec3(depth * 0.6 + 0.2, 0.25, 0.25);
    c.xyz = ss_v_coefs * 0.2 + vec3(0.4);
    if (((gl_GlobalInvocationID.x / 2 + gl_GlobalInvocationID.y / 2) % 2 == 0) && ss_v_coefs.x > 1.0)
        c.x = 1;
    if (((gl_GlobalInvocationID.x / 2 + gl_GlobalInvocationID.y / 2) % 2 == 1) && ss_v_coefs.x < 0.0)
        c.x = 0;
    if (((gl_GlobalInvocationID.x / 2 + gl_GlobalInvocationID.y / 2) % 2 == 0) && ss_v_coefs.y > 1.0)
        c.y = 1;
    if (((gl_GlobalInvocationID.x / 2 + gl_GlobalInvocationID.y / 2) % 2 == 1) && ss_v_coefs.y < 0.0)
        c.y = 0;
    if (((gl_GlobalInvocationID.x / 2 + gl_GlobalInvocationID.y / 2) % 2 == 0) && ss_v_coefs.z > 1.0)
        c.z = 1;
    if (((gl_GlobalInvocationID.x / 2 + gl_GlobalInvocationID.y / 2) % 2 == 1) && ss_v_coefs.z < 0.0)
        c.z = 0;

    //c.rgb = pc_v_coefs;
    c.rgb = ss_v_coefs;
    //c.rgb = vec3(1);

    //if (depth < 0)
    //    return;
    //if (depth < 0 || depth > 1)
    //    return;

    float tcx = dot(vec3(os_v0.x, os_v1.x, os_v2.x), pc_v_coefs);
    float tcy = dot(vec3(os_v0.y, os_v1.y, os_v2.y), pc_v_coefs);
    float tcz = dot(vec3(os_v0.z, os_v1.z, os_v2.z), pc_v_coefs);
    //vec3 tc = vec3(tcx, tcy, tcz);
    //ivec3 tci = ivec3(tc * 16 + 0.001);
    //c.xyz = vec3(1.0, 1.0, tc.x);
    //if ((tci.x + tci.y + tci.z) % 2 == 0)
    //    c.xyz = vec3(1.0, 0.0, tc.y);
    //c.xyz = vec3(tcx, tcy, tcz);

    //c = c * 0.5;
    //vec4 p = imageLoad(renderTarget, ivec2(gl_GlobalInvocationID.xy));
    //c.rgb = c.rgb + p.rgb;
    //c.rgb = mix(c.rgb, p.rgb, 0.5);
    imageStore(renderTarget, ivec2(gl_GlobalInvocationID.xy), c);
}