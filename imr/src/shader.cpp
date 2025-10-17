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

SPIRVModule load_spirv_module(const std::string& filename) {
    size_t size;
    uint32_t* data;
    const char* loc = imr_get_executable_location();
    if (!imr_read_file((std::filesystem::path(loc).parent_path().string() + "/" + filename).c_str(), &size, (unsigned char**) &data))
        throw std::runtime_error("Failed to read " + filename);
    SPIRVModule module;
    module.resize(size / 4);
    memcpy(module.data(), data, size);
    free(data);
    free((char*) loc);
    return module;
}

ReflectedLayout::ReflectedLayout(imr::SPIRVModule& spirv_module, VkShaderStageFlags stage) : stages(stage) {
    auto config = shd_default_compiler_config();
    auto target = shd_default_target_config();

    Module* module = nullptr;
    auto parse_result = shd_parse_spirv(&config, &target, spirv_module.size() * 4, reinterpret_cast<char*>(spirv_module.data()), "imr_module_name_doesnt_matter", &module);
    assert(parse_result == S2S_Success);

    auto globals = shd_module_collect_reachable_globals(module);
    for (size_t i = 0; i < globals.count; i++) {
        auto def = globals.nodes[i];
        auto set = shd_lookup_annotation(def, "DescriptorSet");
        auto binding = shd_lookup_annotation(def, "Binding");

        auto is_acceleration_structure = [&](const Type* type) {
            if (type->tag == ExtType_TAG) {
                ExtType payload = type->payload.ext_type;
                ExtSpvOp op = payload.op->payload.ext_spv_op;
                if (strcmp(op.set, "spirv.core") == 0 && op.opcode == 5341)
                    return true;
            }
            return false;
        };

        const Type* res_type = def->payload.global_variable.type;
        int64_t array_size = 1;
        if (res_type->tag == ArrType_TAG) {
            if (res_type->payload.arr_type.size) {
                array_size = shd_get_int_value(res_type->payload.arr_type.size, false);
            } else {
                // unsized!
                array_size = 0;
            }
            res_type = res_type->payload.arr_type.element_type;
        }

        std::optional<VkDescriptorType> desc_type;
        if (res_type->tag == ImageType_TAG) {
            switch (res_type->payload.image_type.sampled) {
                case 1: desc_type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE; break;
                case 2: desc_type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE; break;
                default: {
                    throw std::runtime_error("Images need to be sampled (1) or storage (2)");
                }
            }
        }
        else if (res_type->tag == SampledImageType_TAG)
            desc_type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        else if (res_type->tag == SamplerType_TAG)
            desc_type = VK_DESCRIPTOR_TYPE_SAMPLER;
        else if (is_acceleration_structure(res_type))
            desc_type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
        else {
            switch (def->payload.global_variable.address_space) {
                case AsPushConstant: {
                    TypeMemLayout layout = shd_get_mem_layout(shd_module_get_arena(module), res_type);
                    push_constants.push_back((VkPushConstantRange) {
                        .stageFlags = stage,
                        .offset = 0,
                        .size = static_cast<uint32_t>(layout.size_in_bytes),
                    });
                    continue;
                }
                case AsInput:
                case AsOutput: break;
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
            if (!set_bindings.contains(seti))
                set_bindings[seti] = std::vector<VkDescriptorSetLayoutBinding>();
            set_bindings[seti].push_back({
                .binding = bindingi,
                .descriptorType = *desc_type,
                .descriptorCount = static_cast<uint32_t>(array_size),
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
    for (auto range_b : b.push_constants) {
        // TODO: we assume no overlap
        push_constants.push_back(range_b);
    }
    // add B to A
    for (auto [set_b, bindings_b] : b.set_bindings) {
        if (set_bindings.contains(set_b)) {
            auto& bindings_a = set_bindings[set_b];
            for (auto binding_b : bindings_b) {
                bool merged = false;
                for (size_t i = 0; i < bindings_a.size(); i++) {
                    auto& binding_a = bindings_a[i];
                    if (binding_a.binding == binding_b.binding) {
                        if (binding_a.descriptorCount != binding_b.descriptorCount || binding_a.descriptorType != binding_b.descriptorType) {
                            throw std::runtime_error("Incompatible bindings");
                        }
                        merged = true;
                    }
                }
                if (!merged)
                    bindings_a.push_back(binding_b);
            }
        } else {
            set_bindings[set_b] = bindings_b;
        }
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
        std::vector<VkDescriptorBindingFlags> flags;
        flags.resize(bindings.size());
        for (auto binding : bindings) {
            if (binding.descriptorCount == 0)
                flags[binding.binding] |= VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT_EXT;
        }

        VkDescriptorSetLayoutBindingFlagsCreateInfo flags_for_bindings_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
            .bindingCount = static_cast<uint32_t>(bindings.size()),
            .pBindingFlags = flags.data()
        };

        CHECK_VK_THROW(vkCreateDescriptorSetLayout(device.device, tmpPtr<VkDescriptorSetLayoutCreateInfo>({
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pNext = &flags_for_bindings_info,
            .bindingCount = static_cast<uint32_t>(bindings.size()),
            .pBindings = bindings.data(),
        }), nullptr, &set_layouts[set]));
    }

    CHECK_VK_THROW(vkCreatePipelineLayout(device.device, tmpPtr<VkPipelineLayoutCreateInfo>({
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

ShaderModule::ShaderModule(imr::Device& device, std::string&& spirv_filename) noexcept(false) {
    auto spirv_module = load_spirv_module(spirv_filename);
    _impl = std::make_unique<Impl>(device, std::move(spirv_module));
}

ShaderModule::Impl::Impl(imr::Device& device, imr::SPIRVModule&& spirv_module) noexcept(false) : device(device), spirv_module(std::move(spirv_module)) {
    assert(this->spirv_module.size() > 0);
    CHECK_VK(vkCreateShaderModule(device.device, tmpPtr<VkShaderModuleCreateInfo>({
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .flags = 0,
            .codeSize = this->spirv_module.size() * 4,
            .pCode = this->spirv_module.data(),
    }), nullptr, &vk_shader_module), throw std::runtime_error("Failed to build shader module"));
}

VkShaderModule ShaderModule::vk_shader_module() const { return _impl->vk_shader_module; }

ShaderModule::Impl::~Impl() {
    vkDestroyShaderModule(device.device, vk_shader_module, nullptr);
}

ShaderModule::~ShaderModule() = default;

ShaderEntryPoint::ShaderEntryPoint(imr::ShaderModule& module, VkShaderStageFlagBits stage, const std::string& entrypoint_name) {
    _impl = std::make_unique<Impl>(module, stage, entrypoint_name);
}

ShaderEntryPoint::Impl::Impl(imr::ShaderModule& module, VkShaderStageFlagBits stage, const std::string& name) : module(module), stage(stage), name(name) {
    reflected = std::make_unique<ReflectedLayout>(module._impl->spirv_module, stage);
}

const std::string& ShaderEntryPoint::name() const { return _impl->name; }

const ShaderModule& ShaderEntryPoint::module() const { return _impl->module; }

VkShaderStageFlagBits ShaderEntryPoint::stage() const { return _impl->stage; }

ShaderEntryPoint::Impl::~Impl() = default;

ShaderEntryPoint::~ShaderEntryPoint() = default;

ComputePipeline::Impl::Impl(imr::Device& device, imr::ShaderEntryPoint& entry_point) : device(device) {
    layout = std::make_unique<PipelineLayout>(device, *entry_point._impl->reflected);

    pipeline = VK_NULL_HANDLE;
    CHECK_VK_THROW(vkCreateComputePipelines(device.device, VK_NULL_HANDLE, 1, tmpPtr<VkComputePipelineCreateInfo>({
            .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .flags = 0,
            .stage = {
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                    .flags = 0,
                    .stage = entry_point.stage(),
                    .module = entry_point._impl->module.vk_shader_module(),
                    .pName = entry_point.name().c_str(),
            },
            .layout = layout->pipeline_layout,
    }), nullptr, &pipeline));
}

ComputePipeline::Impl::Impl(imr::Device& device, std::unique_ptr<ShaderModule>&& module, std::unique_ptr<ShaderEntryPoint>&& ep) : Impl(device, *ep) {
    this->module = std::move(module);
    this->entry_point = std::move(ep);
    assert(this->module && this->entry_point);
}

ComputePipeline::ComputePipeline(imr::Device& device, std::string&& spirv_filename, std::string&& entrypoint_name) {
    auto shader_module = std::make_unique<ShaderModule>(device, std::move(spirv_filename));
    auto entry_point = std::make_unique<ShaderEntryPoint>(*shader_module, VK_SHADER_STAGE_COMPUTE_BIT, entrypoint_name);
    _impl = std::make_unique<ComputePipeline::Impl>(device, std::move(shader_module), std::move(entry_point));
}

ComputePipeline::Impl::~Impl() {
    vkDestroyPipeline(device.device, pipeline, nullptr);
}

VkPipeline ComputePipeline::pipeline() const { return _impl->pipeline; }
VkPipelineLayout ComputePipeline::layout() const { return _impl->layout->pipeline_layout; }
VkDescriptorSetLayout ComputePipeline::set_layout(unsigned i) const { return _impl->layout->set_layouts[i]; }

ComputePipeline::~ComputePipeline() {}

}