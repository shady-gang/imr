#include "imr_private.h"
#include "imr/util.h"

#include <iostream>

namespace imr {

struct FpsCounter::Impl {
    uint64_t last_epoch = imr_get_time_nano();
    int frames_since_last_epoch = 0;

    int fps;
    float avg_frametime;
};

FpsCounter::FpsCounter() {
    _impl = std::make_unique<Impl>();
}

FpsCounter::~FpsCounter() = default;

void FpsCounter::tick() {
    uint64_t now = imr_get_time_nano();
    uint64_t delta = now - _impl->last_epoch;
    if (delta > 1000000000 /* 1 second */) {
        _impl->last_epoch = now;
        if (_impl->frames_since_last_epoch > 0) {
            _impl->fps = _impl->frames_since_last_epoch;
            _impl->avg_frametime = (delta / 1000000000.0f /* scale to seconds */) / _impl->frames_since_last_epoch;
        }
        _impl->frames_since_last_epoch = 0;

        std::cout << average_fps() << ", " << average_frametime() * 1000.0f << "\n";
    }
    _impl->frames_since_last_epoch++;
}

int FpsCounter::average_fps() {
    return _impl->fps;
}

float FpsCounter::average_frametime() {
    return _impl->avg_frametime;
}

void FpsCounter::updateGlfwWindowTitle(GLFWwindow* window) {
    std::string str = "Fps: ";
    str.append(std::to_string(average_fps()));
    str.append(", Avg frametime: ");
    str.append(std::to_string(average_frametime() * 1000.0f));
    str.append("ms");
    glfwSetWindowTitle(window, str.c_str());
}

}
