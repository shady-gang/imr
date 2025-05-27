#include "scene.h"
#include "model.h"
#include "inih/INIReader.h"

#include <iostream>

Scene::Scene(const char* filename) {

    INIReader reader(filename);
    if (reader.ParseError() < 0) {
        std::cerr << "Could not read scene file!\n";
        abort();
    }

    camera_speed = reader.GetInteger("scene", "speed", "1");


}
