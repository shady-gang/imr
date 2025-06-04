#pragma once

#include "imr/imr.h"
#include "imr/util.h"

#include <fstream>
#include <filesystem>

#include "VkBootstrap.h"

#include "initializers.h"

#include "primitives.h"

#include "../../imr/src/shader_private.h"

//#define LAYOUT_REFLECTION

struct MultiStagePipeline {
    Device& device;

    MultiStagePipeline(Device& device) : device(device) {};

    //std::vector<SPIRVModule> spirv_modules;
    std::vector<std::unique_ptr<imr::ShaderModule>> shader_modules;
    std::vector<VkPipelineShaderStageCreateInfo> shader_create_info;
#ifdef LAYOUT_REFLECTION
    std::vector<std::unique_ptr<ReflectedLayout>> reflected_layouts;
#endif

    VkPipelineShaderStageCreateInfo create_shader_info(imr::ShaderModule& shaderModule, VkShaderStageFlagBits stage_bits);
    void load_shader(std::string filename, VkShaderStageFlagBits flags);
};

static bool hasStencilComponent(VkFormat format) {
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

std::unique_ptr<MultiStagePipeline> create_shader_stages(imr::Device& device, bool use_glsl);
VkPipelineLayout create_pipeline_layout(imr::Device& device);

std::unique_ptr<MultiStagePipeline> create_shader_stages_textured(imr::Device& device);
VkPipelineLayout create_pipeline_layout_textured(imr::Device& device, VkDescriptorSetLayout& descriptor_set_layout);

std::unique_ptr<MultiStagePipeline> create_shader_stages_bunny(imr::Device& device);
VkPipelineLayout create_pipeline_layout_bunny(imr::Device& device);

VkPipeline create_pipeline(imr::Device& device, imr::Swapchain& swapchain, VkPipelineLayout& pipeline_layout, std::vector<VkPipelineShaderStageCreateInfo> shader_stages, VkPolygonMode polygon_mode, bool has_tessellation, bool ccw);

VkImageView create_image_view(imr::Device& device, imr::Image& image, VkImageAspectFlags aspectFlags);
