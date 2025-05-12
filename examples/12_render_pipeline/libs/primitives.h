#ifndef RA_PRIMITIVES
#define RA_PRIMITIVES

#include "ra_math.h"
#include "random.h"

struct Ray {
    vec3 origin;
    vec3 dir;
    float tmin = 0, tmax = 99999;
};

// struct Hit {
//     float t;
//     // pls pack well
//     // vec3 p;
//     float u;
//     vec3 n;
//     float v;
// };

struct Hit {
    float t;
    vec2 primary; // aka. barycentric coordinates
    int prim_id;
};

struct Sphere {
    int prim_id;
    int mat_id;
    vec3 center;
    float radius;

    bool intersect(Ray r, Hit&);
};

struct BBox {
    vec3 min, max;

    void intersect_range(Ray r, vec3 ray_inv_dir, vec3 morigin_t_riv, float t[2]);
    bool intersect(Ray r, vec3 ray_inv_dir, Hit&);
    bool contains(vec3 point);
};

struct Triangle {
    int prim_id;
    int mat_id;
    vec3 v0, v1, v2;
    vec3 n0, n1, n2;
    vec2 t0, t1, t2;

    bool intersect(Ray r, Hit&);
    vec3 get_face_normal() const;
    vec3 get_position(vec2 bary) const;
    vec3 get_vertex_normal(vec2 bary) const;
    vec2 get_texcoords(vec2 bary) const;
    float get_area() const;
    vec2 sample_point_on_surface(RNGState* rng);
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

#endif
