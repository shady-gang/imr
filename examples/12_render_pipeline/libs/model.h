#pragma once

#include "imr/imr.h"

//#include "host.h"
#include "primitives.h"
#include "material.h"
//#include "emitter.h"
#include "camera.h"

#include <string>

struct Model {
    Model(const char* path, imr::Device&);
    ~Model();

    std::vector<Triangle> triangles;
    std::unique_ptr<imr::Buffer> triangles_gpu;

    std::vector<Material> materials;
    //shady::Buffer* materials_gpu = nullptr;

    // Note: The first entry is the environment constant color
    //std::vector<Emitter> emitters;
    //shady::Buffer* emitters_gpu;

    Camera loaded_camera;
};
