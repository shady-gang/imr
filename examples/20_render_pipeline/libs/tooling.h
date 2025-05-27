#pragma once

#include "imr/imr.h"
#include "imr/util.h"

#include <fstream>
#include <filesystem>

#include "VkBootstrap.h"

#include "initializers.h"

#include "primitives.h"

static bool hasStencilComponent(VkFormat format) {
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

std::vector<VkPipelineShaderStageCreateInfo> create_shader_stages(imr::Device& device, bool use_glsl);
VkPipelineLayout create_pipeline_layout(imr::Device& device);
VkPipeline create_pipeline(imr::Device& device, imr::Swapchain& swapchain, VkPipelineLayout& pipeline_layout, std::vector<VkPipelineShaderStageCreateInfo> shader_stages, VkPolygonMode polygon_mode, bool has_tessellation);

VkImageView create_image_view(imr::Device& device, imr::Image& image, VkImageAspectFlags aspectFlags);

std::vector<VkPipelineShaderStageCreateInfo> create_shader_stages_bunny(imr::Device& device);
VkPipelineLayout create_pipeline_layout_bunny(imr::Device& device);

enum RENDER_MODE {
    FILL,
    GRID,
};

struct STATE_UPDATE {
    RENDER_MODE * render_mode;
    float * fog_dropoff_lower;
    float * fog_dropoff_upper;
    int * fog_power;
    float * fog_lower_old;
    float * fog_upper_old;
    int * fog_power_old;
    float * tess_factor;
    bool * update_tess;
    bool * toggle_flight;
};
