#ifndef IMR_SHADER_PRIVATE_H
#define IMR_SHADER_PRIVATE_H

#include "imr_private.h"

namespace imr {

struct SPIRVModule {
    size_t size = 0;
    uint32_t* data = nullptr;
    std::string name;

    SPIRVModule(const std::string& filename) noexcept(false);
    ~SPIRVModule();
};

/// Generates set layouts and pipeline layouts from the SPIR-V module by parsing it as a shady module and using the IR inspection API to find bindings and such
struct ReflectedLayout {
    VkShaderStageFlags stages;
    std::unordered_map<int, std::vector<VkDescriptorSetLayoutBinding>> set_bindings;
    std::vector<VkPushConstantRange> push_constants;

    ReflectedLayout(SPIRVModule& spirv_module, VkShaderStageFlags stage);
    ReflectedLayout(ReflectedLayout& a, ReflectedLayout& b);
};

/// Turns the ReflectedLayout into the VkDescriptorSetLayout s and VkPipelineLayout
struct PipelineLayout {
    imr::Device& device;

    std::vector<VkDescriptorSetLayout> set_layouts;
    VkPipelineLayout pipeline_layout;

    PipelineLayout(imr::Device& device, ReflectedLayout& reflected_layout);
    ~PipelineLayout();
};

struct ShaderModule {
    imr::Device& device;
    VkShaderModule vk_shader_module;

    ShaderModule(imr::Device& device, SPIRVModule& spirv_module) noexcept(false);

    ShaderModule(const ShaderModule&) = delete;
    ShaderModule(ShaderModule&&) = default;

    ~ShaderModule();
};

struct ComputeShader::Impl {
    Device& device;
    std::unique_ptr<ReflectedLayout> reflected;
    std::unique_ptr<ShaderModule> shader_module;
    std::unique_ptr<PipelineLayout> layout;
    VkPipeline pipeline;

    Impl(imr::Device& device, std::string&& name, std::string&& entrypoint_name);
    ~Impl();
};

}

#endif
