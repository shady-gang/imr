#pragma once

#include "imr/imr.h"
#include "imr/util.h"

#include <fstream>
#include <filesystem>

#include "VkBootstrap.h"

#include "initializers.h"

#include "libs/camera.h"
//#include "libs/model.h"

struct Vertex {
    vec3 pos;
    vec3 color;

    static VkVertexInputBindingDescription getBindingDescription() {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        return bindingDescription;
    }

    static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions() {
        std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions{};

        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(Vertex, pos);

        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Vertex, color);

        return attributeDescriptions;
    }
};


static bool hasStencilComponent(VkFormat format) {
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

std::vector<VkPipelineShaderStageCreateInfo> create_shader_stages(imr::Device& device, bool use_glsl);
VkPipelineLayout create_pipeline_layout(imr::Device& device);
VkPipeline create_pipeline(imr::Device& device, imr::Swapchain& swapchain, VkPipelineLayout& pipeline_layout, std::vector<VkPipelineShaderStageCreateInfo> shader_stages, VkPolygonMode polygon_mode);

VkImageView create_image_view(imr::Device& device, imr::Image& image, VkImageAspectFlags aspectFlags);
