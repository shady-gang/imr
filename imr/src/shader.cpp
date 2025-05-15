#include "shader_private.h"

#include "imr/imr.h"
#include "imr/util.h"

extern "C" {

#include "shady/ir/arena.h"
#include "shady/ir/module.h"
#include "shady/ir/memory_layout.h"
#include "shady/fe/spirv.h"
#include "shady/driver.h"

}

#include <filesystem>

namespace imr {

SPIRVModule::SPIRVModule(const std::string& filename) : name(filename) {
    if (!imr_read_file((std::filesystem::path(imr_get_executable_location()).parent_path().string() + "/" + filename).c_str(), &size, (unsigned char**) &data))
    throw std::runtime_error("Failed to read " + filename);
}

SPIRVModule::~SPIRVModule() {
    free(data);
}

ReflectedLayout::ReflectedLayout(imr::SPIRVModule& spirv_module, VkShaderStageFlags stage) : stages(stage) {
    auto config = shd_default_compiler_config();
    auto target = shd_default_target_config();

    Module* module = nullptr;
    auto parse_result = shd_parse_spirv(&config, &target, spirv_module.size, reinterpret_cast<char*>(spirv_module.data), spirv_module.name.c_str(), &module);
    assert(parse_result == S2S_Success);

    auto globals = shd_module_collect_reachable_globals(module);
    for (size_t i = 0; i < globals.count; i++) {
        auto def = globals.nodes[i];
        auto set = shd_lookup_annotation(def, "DescriptorSet");
        auto binding = shd_lookup_annotation(def, "Binding");

        std::optional<VkDescriptorType> desc_type;
        if (def->payload.global_variable.type->tag == ImageType_TAG)
            desc_type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        else if (def->payload.global_variable.type->tag == SampledImageType_TAG)
            desc_type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        else if (def->payload.global_variable.type->tag == SamplerType_TAG)
            desc_type = VK_DESCRIPTOR_TYPE_SAMPLER;
        else {
            switch (def->payload.global_variable.address_space) {
                case AsPushConstant: {
                    TypeMemLayout layout = shd_get_mem_layout(shd_module_get_arena(module), def->payload.global_variable.type);
                    push_constants.push_back((VkPushConstantRange) {
                        .stageFlags = stage,
                        .offset = 0,
                        .size = static_cast<uint32_t>(layout.size_in_bytes),
                    });
                    continue;
                }
                case AsInput:
                case AsOutput: throw std::runtime_error("TODO");
                case AsShaderStorageBufferObject:
                    desc_type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                    break;
                case AsUniform:
                case AsUniformConstant:
                    desc_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                    break;
                default: break;
            }
        }

        if (desc_type) {
            assert(binding && set);
            uint32_t seti = shd_get_int_value(shd_get_annotation_value(set), false);
            uint32_t bindingi = shd_get_int_value(shd_get_annotation_value(binding), false);
            uint32_t count = 1;
            if (!set_bindings.contains(seti))
                set_bindings[seti] = std::vector<VkDescriptorSetLayoutBinding>();
            set_bindings[seti].push_back((VkDescriptorSetLayoutBinding) {
                .binding = bindingi,
                .descriptorType = *desc_type,
                .descriptorCount = count,
                .stageFlags = stage,
            });
        }
    }

    auto a = shd_module_get_arena(module);
    shd_destroy_ir_arena(a);
}

ReflectedLayout::ReflectedLayout(imr::ReflectedLayout& a, imr::ReflectedLayout& b) : push_constants(a.push_constants), set_bindings(a.set_bindings), stages(a.stages | b.stages) {
    if ((a.stages & b.stages) != 0)
        throw std::runtime_error("Overlap in stages");
    for (auto [key, value] : set_bindings) {
        throw std::runtime_error("TODO");
    }
}

PipelineLayout::PipelineLayout(imr::Device& device, imr::ReflectedLayout& reflected_layout) : device(device) {
    int max_set = 0;
    for (auto& [set, value] : reflected_layout.set_bindings) {
        if (set > max_set)
            max_set = set;
    }
    assert(max_set < 32);
    set_layouts.resize(max_set + 1);
    for (unsigned set = 0; set < max_set + 1; set++) {
        auto& bindings = reflected_layout.set_bindings[set];
        CHECK_VK_THROW(vkCreateDescriptorSetLayout(device.device, tmpPtr((VkDescriptorSetLayoutCreateInfo) {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = static_cast<uint32_t>(bindings.size()),
            .pBindings = bindings.data(),
        }), nullptr, &set_layouts[set]));
    }

    CHECK_VK_THROW(vkCreatePipelineLayout(device.device, tmpPtr((VkPipelineLayoutCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = static_cast<uint32_t>(set_layouts.size()),
        .pSetLayouts = set_layouts.data(),
        .pushConstantRangeCount = static_cast<uint32_t>(reflected_layout.push_constants.size()),
        .pPushConstantRanges = reflected_layout.push_constants.data()
    }), nullptr, &pipeline_layout));
}

PipelineLayout::~PipelineLayout() {
    vkDestroyPipelineLayout(device.device, pipeline_layout, nullptr);
    for (auto set_layout : set_layouts)
        vkDestroyDescriptorSetLayout(device.device, set_layout, nullptr);
}

ShaderModule::ShaderModule(imr::Device& device, imr::SPIRVModule& spirv_module) noexcept(false) : device(device) {
    CHECK_VK(vkCreateShaderModule(device.device, tmpPtr((VkShaderModuleCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .flags = 0,
        .codeSize = spirv_module.size,
        .pCode = spirv_module.data,
    }), nullptr, &vk_shader_module), throw std::runtime_error("Failed to build shader module"));
}

ShaderModule::~ShaderModule() {
    vkDestroyShaderModule(device.device, vk_shader_module, nullptr);
}

ComputeShader::Impl::Impl(imr::Device& device, std::string&& name, std::string&& entrypoint_name) : device(device) {
    auto shader_stage = VK_SHADER_STAGE_COMPUTE_BIT;
    auto spirv_module = SPIRVModule(name);
    reflected = std::make_unique<ReflectedLayout>(spirv_module, shader_stage);
    shader_module = std::make_unique<ShaderModule>(device, spirv_module);
    layout = std::make_unique<PipelineLayout>(device, *reflected);

    CHECK_VK_THROW(vkCreateComputePipelines(device.device, VK_NULL_HANDLE, 1, tmpPtr((VkComputePipelineCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .flags = 0,
        .stage = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .flags = 0,
            .stage = shader_stage,
            .module = shader_module->vk_shader_module,
            .pName = entrypoint_name.c_str(),
        },
        .layout = layout->pipeline_layout,
    }), nullptr, &pipeline));
}

ComputeShader::Impl::~Impl() {
    vkDestroyPipeline(device.device, pipeline, nullptr);
}

ComputeShader::ComputeShader(imr::Device& device, std::string&& name, std::string&& entrypoint_name) {
    _impl = std::make_unique<ComputeShader::Impl>(device, std::move(name), std::move(entrypoint_name));
}

VkPipeline ComputeShader::pipeline() const { return _impl->pipeline; }
VkPipelineLayout ComputeShader::layout() const { return _impl->layout->pipeline_layout; }
VkDescriptorSetLayout ComputeShader::set_layout(unsigned i) const { return _impl->layout->set_layouts[i]; }

ComputeShader::~ComputeShader() {}

}