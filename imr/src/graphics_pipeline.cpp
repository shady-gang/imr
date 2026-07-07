#include "shader_private.h"

namespace imr {

template<typename T>
T* optional_to_ptr(std::optional<T>& o) {
    if (o)
        return &o.value();
    return nullptr;
}

GraphicsPipeline::GraphicsPipeline(imr::Device& d, std::vector<ShaderEntryPoint*>&& stages, RenderTargetsState rts, imr::GraphicsPipeline::StateBuilder state) {
    _impl = std::make_unique<Impl>(d, std::move(stages), rts, state);
}

GraphicsPipeline::Impl::Impl(Device& device, std::vector<ShaderEntryPoint*>&& stages, RenderTargetsState render_targets, StateBuilder state) : device_(device) {
    std::vector<VkPipelineShaderStageCreateInfo> vk_stages;
    std::vector<std::unique_ptr<VkPipelineShaderStageRequiredSubgroupSizeCreateInfo>> subgroup_sizes;
    VkShaderStageFlags conflicts = 0;
    std::optional<ReflectedLayout> merged_layout;
    for (auto stage : stages) {
        if (conflicts & stage->stage())
            throw std::runtime_error("Duplicated stages");
        VkPipelineShaderStageCreateInfo vk_stage = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = stage->stage(),
            .module = stage->module().vk_shader_module(),
            .pName = stage->name().c_str(),
        };

        switch (stage->stage()) {
            case VK_SHADER_STAGE_MESH_BIT_EXT:
            case VK_SHADER_STAGE_TASK_BIT_EXT:
            case VK_SHADER_STAGE_COMPUTE_BIT: {
                auto& ss = subgroup_sizes.emplace_back(std::make_unique<VkPipelineShaderStageRequiredSubgroupSizeCreateInfo>());
                *ss = {
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_REQUIRED_SUBGROUP_SIZE_CREATE_INFO,
                    .requiredSubgroupSize = 32,
                };
                appendPNext((VkBaseOutStructure*) &vk_stage, (VkBaseOutStructure*) &*ss);
            }
            default: break;
        }

        vk_stages.push_back(vk_stage);
        if (!merged_layout)
            merged_layout = *stage->_impl->reflected;
        else
            merged_layout = ReflectedLayout(*merged_layout, *stage->_impl->reflected);
    }

    layout = std::make_unique<PipelineLayout>(device, *merged_layout);
    final_layout = *merged_layout;

    std::vector<VkDynamicState> dynamic_states = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
        //VK_DYNAMIC_STATE_CULL_MODE,
        //VK_DYNAMIC_STATE_FRONT_FACE,
    };

    VkPipelineDynamicStateCreateInfo dynamic_state {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,

        .dynamicStateCount = static_cast<uint32_t>(dynamic_states.size()),
        .pDynamicStates = dynamic_states.data(),
    };

    std::vector<VkFormat> color_formats;
    std::vector<VkPipelineColorBlendAttachmentState> color_attachments_blending;

    for (auto& color_rt : render_targets.color) {
        color_formats.push_back(color_rt.format);
        color_attachments_blending.push_back(color_rt.blending);
    }

    VkPipelineRenderingCreateInfo rendertargets_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = static_cast<uint32_t>(color_formats.size()),
        .pColorAttachmentFormats = color_formats.data(),
    };

    if (render_targets.depth) {
        rendertargets_state.depthAttachmentFormat = render_targets.depth->format;
    }

    VkPipelineColorBlendStateCreateInfo blend_state = render_targets.all_targets_blend_state;
    blend_state.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend_state.attachmentCount = color_formats.size();
    blend_state.pAttachments = color_attachments_blending.data();

    VkGraphicsPipelineCreateInfo pipeline_create_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,

        .flags = 0*VK_PIPELINE_CREATE_DISABLE_OPTIMIZATION_BIT,
        .stageCount = static_cast<uint32_t>(vk_stages.size()),
        .pStages = vk_stages.data(),
        .pVertexInputState = optional_to_ptr(state.vertexInputState),
        .pInputAssemblyState = optional_to_ptr(state.inputAssemblyState),
        .pTessellationState = optional_to_ptr(state.tessellationState),
        .pViewportState = optional_to_ptr(state.viewportState),
        .pRasterizationState = optional_to_ptr(state.rasterizationState),
        .pMultisampleState = optional_to_ptr(state.multisampleState),
        .pDepthStencilState = optional_to_ptr(state.depthStencilState),
        .pColorBlendState = &blend_state,
        .pDynamicState = &dynamic_state,
        .layout = layout->pipeline_layout,
        .renderPass = nullptr,
    };

    appendPNext((VkBaseOutStructure*) &pipeline_create_info, (VkBaseOutStructure*) &rendertargets_state);

    CHECK_VK_THROW(vkCreateGraphicsPipelines(device_.device, VK_NULL_HANDLE, 1, &pipeline_create_info, VK_NULL_HANDLE, &pipeline));
}

GraphicsPipeline::Impl::~Impl() {
    vkDestroyPipeline(device_.device, pipeline, VK_NULL_HANDLE);
}

GraphicsPipeline::~GraphicsPipeline() = default;

VkPipelineLayout GraphicsPipeline::layout() const { return _impl->layout->pipeline_layout; }
VkDescriptorSetLayout GraphicsPipeline::set_layout(unsigned int i) const { return _impl->layout->set_layouts[i]; }
VkPipeline GraphicsPipeline::pipeline() const { return _impl->pipeline; }

VkPipelineVertexInputStateCreateInfo GraphicsPipeline::no_vertex_input() {
    VkPipelineVertexInputStateCreateInfo vertex_input {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    };
    return vertex_input;
}

VkPipelineInputAssemblyStateCreateInfo GraphicsPipeline::simple_triangle_input_assembly() {
    VkPipelineInputAssemblyStateCreateInfo input_assembly {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    };
    return input_assembly;
}

VkPipelineViewportStateCreateInfo GraphicsPipeline::one_dynamically_sized_viewport() {
    VkPipelineViewportStateCreateInfo viewports {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1,
    };
    return viewports;
}

VkPipelineRasterizationStateCreateInfo GraphicsPipeline::solid_filled_polygons() {
    VkPipelineRasterizationStateCreateInfo rasterization {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,

        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_CLOCKWISE,

        .lineWidth = 1.0f,
    };
    return rasterization;
}

VkPipelineMultisampleStateCreateInfo GraphicsPipeline::one_spp() {
    VkPipelineMultisampleStateCreateInfo multisample {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    };
    return multisample;
}

VkPipelineDepthStencilStateCreateInfo GraphicsPipeline::simple_depth_testing() {
    VkPipelineDepthStencilStateCreateInfo depth_stencil {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,

        .depthTestEnable = VK_TRUE,
        .depthWriteEnable = VK_TRUE,
        .depthCompareOp = VK_COMPARE_OP_LESS
    };
    return depth_stencil;
}

}
