#pragma once

#include "imr/imr.h"
#include "imr/util.h"

#include "ra_math.h"

#include <nasl.h>
#include <nasl_mat.h>

using namespace imr;
using namespace nasl;

struct Triangle {
    int prim_id;
    int mat_id;
    vec3 v0, v1, v2;
    vec3 n0, n1, n2;
    vec2 t0, t1, t2;

    //bool intersect(Ray r, Hit&);
    vec3 get_face_normal() const;
    vec3 get_position(vec2 bary) const;
    vec3 get_vertex_normal(vec2 bary) const;
    vec2 get_texcoords(vec2 bary) const;
    float get_area() const;
    //vec2 sample_point_on_surface(RNGState* rng);
};

struct Vertex {
    vec3 pos;
    vec3 normal;
    vec3 color;

    static VkVertexInputBindingDescription getBindingDescription() {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        return bindingDescription;
    }

    static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions() {
        std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};

        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(Vertex, pos);

        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Vertex, normal);

        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[2].offset = offsetof(Vertex, color);

        return attributeDescriptions;
    }
};
