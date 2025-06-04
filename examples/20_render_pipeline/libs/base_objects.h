#pragma once

#include <nasl.h>
#include <nasl_mat.h>

#include "../libs/primitives.h"

using namespace nasl;

const vec3 vertex_1_color = {1.0f, 0.0f, 0.0f};
const vec3 vertex_2_color = {0.0f, 1.0f, 0.0f};
const vec3 vertex_3_color = {1.0f, 1.0f, 0.0f};
const vec3 vertex_4_color = {0.0f, 0.0f, 1.0f};
const vec3 vertex_5_color = {1.0f, 0.0f, 1.0f};
const vec3 vertex_6_color = {0.0f, 1.0f, 1.0f};
const vec3 vertex_7_color = {1.0f, 1.0f, 1.0f};

const std::vector<Vertex> cube_vertices = {
    //face 1
    {{-0.5f, -0.5f,  0.5f}, {0, 0, 0}, {0, 0}, vertex_1_color},
    {{ 0.5f,  0.5f,  0.5f}, {0, 0, 0}, {0, 0}, vertex_2_color},
    {{-0.5f,  0.5f,  0.5f}, {0, 0, 0}, {0, 0}, vertex_3_color},

    {{-0.5f, -0.5f,  0.5f}, {0, 0, 0}, {0, 0}, vertex_1_color},
    {{ 0.5f, -0.5f,  0.5f}, {0, 0, 0}, {0, 0}, vertex_2_color},
    {{ 0.5f,  0.5f,  0.5f}, {0, 0, 0}, {0, 0}, vertex_3_color},

    //face 2
    {{ 0.5f,  0.5f, -0.5f}, {0, 0, 0}, {0, 0}, vertex_1_color},
    {{-0.5f, -0.5f, -0.5f}, {0, 0, 0}, {0, 0}, vertex_2_color},
    {{-0.5f,  0.5f, -0.5f}, {0, 0, 0}, {0, 0}, vertex_3_color},

    {{ 0.5f, -0.5f, -0.5f}, {0, 0, 0}, {0, 0}, vertex_1_color},
    {{-0.5f, -0.5f, -0.5f}, {0, 0, 0}, {0, 0}, vertex_2_color},
    {{ 0.5f,  0.5f, -0.5f}, {0, 0, 0}, {0, 0}, vertex_3_color},

    //face 3
    {{ 0.5f,  0.5f,  0.5f}, {0, 0, 0}, {0, 0}, vertex_1_color},
    {{-0.5f,  0.5f, -0.5f}, {0, 0, 0}, {0, 0}, vertex_2_color},
    {{-0.5f,  0.5f,  0.5f}, {0, 0, 0}, {0, 0}, vertex_3_color},

    {{ 0.5f,  0.5f, -0.5f}, {0, 0, 0}, {0, 0}, vertex_1_color},
    {{-0.5f,  0.5f, -0.5f}, {0, 0, 0}, {0, 0}, vertex_2_color},
    {{ 0.5f,  0.5f,  0.5f}, {0, 0, 0}, {0, 0}, vertex_3_color},

    //face 4
    {{ 0.5f, -0.5f,  0.5f}, {0, 0, 0}, {0, 0}, vertex_1_color},
    {{-0.5f, -0.5f,  0.5f}, {0, 0, 0}, {0, 0}, vertex_2_color},
    {{-0.5f, -0.5f, -0.5f}, {0, 0, 0}, {0, 0}, vertex_3_color},

    {{ 0.5f, -0.5f, -0.5f}, {0, 0, 0}, {0, 0}, vertex_1_color},
    {{ 0.5f, -0.5f,  0.5f}, {0, 0, 0}, {0, 0}, vertex_2_color},
    {{-0.5f, -0.5f, -0.5f}, {0, 0, 0}, {0, 0}, vertex_3_color},

    //face 5
    {{ 0.5f,  0.5f,  0.5f}, {0, 0, 0}, {0, 0}, vertex_1_color},
    {{ 0.5f, -0.5f,  0.5f}, {0, 0, 0}, {0, 0}, vertex_2_color},
    {{ 0.5f, -0.5f, -0.5f}, {0, 0, 0}, {0, 0}, vertex_3_color},

    {{ 0.5f,  0.5f, -0.5f}, {0, 0, 0}, {0, 0}, vertex_1_color},
    {{ 0.5f,  0.5f,  0.5f}, {0, 0, 0}, {0, 0}, vertex_2_color},
    {{ 0.5f, -0.5f, -0.5f}, {0, 0, 0}, {0, 0}, vertex_3_color},

    //face 6
    {{-0.5f,  0.5f,  0.5f}, {0, 0, 0}, {0, 0}, vertex_1_color},
    {{-0.5f, -0.5f, -0.5f}, {0, 0, 0}, {0, 0}, vertex_2_color},
    {{-0.5f, -0.5f,  0.5f}, {0, 0, 0}, {0, 0}, vertex_3_color},

    {{-0.5f,  0.5f,  0.5f}, {0, 0, 0}, {0, 0}, vertex_1_color},
    {{-0.5f,  0.5f, -0.5f}, {0, 0, 0}, {0, 0}, vertex_2_color},
    {{-0.5f, -0.5f, -0.5f}, {0, 0, 0}, {0, 0}, vertex_3_color},

    //face 1
    {{-0.5f, -0.5f,  0.5f-2.0f}, {0, 0, 0}, {0, 0}, vertex_4_color},
    {{ 0.5f,  0.5f,  0.5f-2.0f}, {0, 0, 0}, {0, 0}, vertex_5_color},
    {{-0.5f,  0.5f,  0.5f-2.0f}, {0, 0, 0}, {0, 0}, vertex_6_color},

    {{-0.5f, -0.5f,  0.5f-2.0f}, {0, 0, 0}, {0, 0}, vertex_4_color},
    {{ 0.5f, -0.5f,  0.5f-2.0f}, {0, 0, 0}, {0, 0}, vertex_5_color},
    {{ 0.5f,  0.5f,  0.5f-2.0f}, {0, 0, 0}, {0, 0}, vertex_6_color},

    //face 2
    {{ 0.5f,  0.5f, -0.5f-2.0f}, {0, 0, 0}, {0, 0}, vertex_4_color},
    {{-0.5f, -0.5f, -0.5f-2.0f}, {0, 0, 0}, {0, 0}, vertex_5_color},
    {{-0.5f,  0.5f, -0.5f-2.0f}, {0, 0, 0}, {0, 0}, vertex_6_color},

    {{ 0.5f, -0.5f, -0.5f-2.0f}, {0, 0, 0}, {0, 0}, vertex_4_color},
    {{-0.5f, -0.5f, -0.5f-2.0f}, {0, 0, 0}, {0, 0}, vertex_5_color},
    {{ 0.5f,  0.5f, -0.5f-2.0f}, {0, 0, 0}, {0, 0}, vertex_6_color},

    //face 3
    {{ 0.5f,  0.5f,  0.5f-2.0f}, {0, 0, 0}, {0, 0}, vertex_4_color},
    {{-0.5f,  0.5f, -0.5f-2.0f}, {0, 0, 0}, {0, 0}, vertex_5_color},
    {{-0.5f,  0.5f,  0.5f-2.0f}, {0, 0, 0}, {0, 0}, vertex_6_color},

    {{ 0.5f,  0.5f, -0.5f-2.0f}, {0, 0, 0}, {0, 0}, vertex_4_color},
    {{-0.5f,  0.5f, -0.5f-2.0f}, {0, 0, 0}, {0, 0}, vertex_5_color},
    {{ 0.5f,  0.5f,  0.5f-2.0f}, {0, 0, 0}, {0, 0}, vertex_6_color},

    //face 4
    {{ 0.5f, -0.5f,  0.5f-2.0f}, {0, 0, 0}, {0, 0}, vertex_4_color},
    {{-0.5f, -0.5f,  0.5f-2.0f}, {0, 0, 0}, {0, 0}, vertex_5_color},
    {{-0.5f, -0.5f, -0.5f-2.0f}, {0, 0, 0}, {0, 0}, vertex_6_color},

    {{ 0.5f, -0.5f, -0.5f-2.0f}, {0, 0, 0}, {0, 0}, vertex_4_color},
    {{ 0.5f, -0.5f,  0.5f-2.0f}, {0, 0, 0}, {0, 0}, vertex_5_color},
    {{-0.5f, -0.5f, -0.5f-2.0f}, {0, 0, 0}, {0, 0}, vertex_6_color},

    //face 5
    {{ 0.5f,  0.5f,  0.5f-2.0f}, {0, 0, 0}, {0, 0}, vertex_4_color},
    {{ 0.5f, -0.5f,  0.5f-2.0f}, {0, 0, 0}, {0, 0}, vertex_5_color},
    {{ 0.5f, -0.5f, -0.5f-2.0f}, {0, 0, 0}, {0, 0}, vertex_6_color},

    {{ 0.5f,  0.5f, -0.5f-2.0f}, {0, 0, 0}, {0, 0}, vertex_4_color},
    {{ 0.5f,  0.5f,  0.5f-2.0f}, {0, 0, 0}, {0, 0}, vertex_5_color},
    {{ 0.5f, -0.5f, -0.5f-2.0f}, {0, 0, 0}, {0, 0}, vertex_6_color},

    //face 6
    {{-0.5f,  0.5f,  0.5f-2.0f}, {0, 0, 0}, {0, 0}, vertex_4_color},
    {{-0.5f, -0.5f, -0.5f-2.0f}, {0, 0, 0}, {0, 0}, vertex_5_color},
    {{-0.5f, -0.5f,  0.5f-2.0f}, {0, 0, 0}, {0, 0}, vertex_6_color},

    {{-0.5f,  0.5f,  0.5f-2.0f}, {0, 0, 0}, {0, 0}, vertex_4_color},
    {{-0.5f,  0.5f, -0.5f-2.0f}, {0, 0, 0}, {0, 0}, vertex_5_color},
    {{-0.5f, -0.5f, -0.5f-2.0f}, {0, 0, 0}, {0, 0}, vertex_6_color},
};

void create_flat_surface(std::vector<Vertex> & data, int tessellation) {
    //float GRID_SIZE = 1.0f / tessellation;
    float GRID_SIZE = 1.0f;
    for (int xi = -tessellation ; xi < tessellation; xi++) {
        for (int zi = -tessellation ; zi < tessellation; zi++) {
            Vertex a = {{(xi + 1) * GRID_SIZE,  0.f, (zi + 1) * GRID_SIZE}, {0, -1, 0}, {0, 0}, vertex_2_color};
            Vertex b = {{     xi  * GRID_SIZE,  0.f, (zi + 1) * GRID_SIZE}, {0, -1, 0}, {0, 0}, vertex_2_color};
            Vertex c = {{(xi + 1) * GRID_SIZE,  0.f,      zi  * GRID_SIZE}, {0, -1, 0}, {0, 0}, vertex_2_color};
            Vertex d = {{     xi  * GRID_SIZE,  0.f,      zi  * GRID_SIZE}, {0, -1, 0}, {0, 0}, vertex_2_color};
            data.push_back(a);
            data.push_back(b);
            data.push_back(d);
            data.push_back(a);
            data.push_back(d);
            data.push_back(c);
        }
    }
}
