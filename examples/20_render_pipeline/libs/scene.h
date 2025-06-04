#pragma once

#include "imr/imr.h"
#include "imr/util.h"
#include "camera.h"

#include <nasl.h>
#include <nasl_mat.h>

enum RENDER_MODE {
    FILL,
    GRID,
};

struct PipelineStep {
    bool is_tessellated;
    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline_fill;
    VkPipeline pipeline_grid;
    std::unique_ptr<imr::Buffer> vertex_buffer;
    mat4 object_transformation;

    std::unique_ptr<imr::Image> uv_texture_image;
    VkImageView texture_image_view;
    VkDescriptorSetLayout descriptor_set_layout;
    VkDescriptorPool descriptorPool;
    VkDescriptorSet descriptorSet;
    VkSampler texture_sampler;
};

struct Scene {
    Scene (const char* filename, imr::Device& device, imr::Swapchain& swapchain, bool use_glsl);
    ~Scene();

    imr::Device& device;

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
    bool tess_landscape;

    bool flight;
    uint64_t start_time = imr_get_time_nano();
    double time_offset = 0;
    mat4 camera_control_matrix;

    RENDER_MODE render_mode;

    double get_timestep();
    void set_camera_to_timestep(imr::Image& image);

    void render_to_cmdbuf(VkCommandBuffer& cmdbuf, imr::Image& image, imr::Swapchain::Frame& frame);

    std::vector<PipelineStep> render_steps;
};
