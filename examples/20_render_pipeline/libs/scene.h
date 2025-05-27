#pragma once

#include "imr/imr.h"
#include "imr/util.h"
#include "camera.h"

enum RENDER_MODE {
    FILL,
    GRID,
};

struct Scene {

    Camera camera;
    CameraFreelookState camera_state = {
        .fly_speed = 1.0f,
        .mouse_sensitivity = 1,
    };
    float fog_dropoff_lower;
    float fog_dropoff_upper;
    int fog_power;
    float fog_lower_old;
    float fog_upper_old;
    int fog_power_old;
    
    float tess_factor;
    bool update_tess = true;

    bool flight;
    uint64_t start_time = imr_get_time_nano();
    double time_offset = 0;
    mat4 camera_control_matrix;

    RENDER_MODE render_mode;


    double get_timestep() {
        int runtime = 30;
        uint64_t nano_time = imr_get_time_nano() - start_time;
        double time_step = fmod((double) nano_time / 1000 / 1000 / 1000 / runtime, 1) - time_offset;
        return time_step;
    }

};
