#include "tooling.h"

#include <iostream>

using namespace imr;

VkPipelineShaderStageCreateInfo MultiStagePipeline::create_shader_info(ShaderModule& shaderModule, VkShaderStageFlagBits stage_bits) {
    VkPipelineShaderStageCreateInfo shaderStageInfo{};
    shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageInfo.stage = stage_bits;
    shaderStageInfo.module = shaderModule.vk_shader_module;
    shaderStageInfo.pName = "main";
    return shaderStageInfo;
}

void MultiStagePipeline::load_shader(std::string filename, VkShaderStageFlagBits flags) {
    auto spv = SPIRVModule(filename);
    auto& module = shader_modules.emplace_back(std::make_unique<ShaderModule>(device, spv));
    shader_create_info.emplace_back(create_shader_info(*module, flags));
#ifdef LAYOUT_REFLECTION
    reflected_layouts.emplace_back(std::make_unique<ReflectedLayout>(spv, flags));
#endif
}

std::unique_ptr<MultiStagePipeline> create_shader_stages(Device& device, bool use_glsl) {
    std::unique_ptr<MultiStagePipeline> pipeline = std::make_unique<MultiStagePipeline>(device);

    pipeline->load_shader(std::string("shaders/shader.plane.vert") + (use_glsl ? "" : ".cpp") + ".spv", VK_SHADER_STAGE_VERTEX_BIT);
    pipeline->load_shader(std::string("shaders/shader.plane.frag") + (use_glsl ? "" : ".cpp") + ".spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    pipeline->load_shader("shaders/shader.plane.tesc.spv", VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
    pipeline->load_shader("shaders/shader.plane.tese.spv", VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);

    return pipeline;
}

std::unique_ptr<MultiStagePipeline> create_shader_stages_bunny(Device& device) {
    std::unique_ptr<MultiStagePipeline> pipeline = std::make_unique<MultiStagePipeline>(device);

    pipeline->load_shader("shaders/shader.model.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    pipeline->load_shader("shaders/shader.model.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

    return pipeline;
}

std::unique_ptr<MultiStagePipeline> create_shader_stages_textured(Device& device) {
    std::unique_ptr<MultiStagePipeline> pipeline = std::make_unique<MultiStagePipeline>(device);

    pipeline->load_shader("shaders/shader.tex.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    pipeline->load_shader("shaders/shader.tex.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

    return pipeline;
}

VkPipelineLayout create_pipeline_layout_textured(Device& device, VkDescriptorSetLayout &descriptor_set_layout) {
    std::vector<VkPushConstantRange> ranges;

    VkPushConstantRange vertex_range = initializers::push_constant_range(
            VK_SHADER_STAGE_VERTEX_BIT,
            32 * 4, //mat4 should™ have this size
            0
    );
    ranges.push_back(vertex_range);

    VkPushConstantRange frag_range = initializers::push_constant_range(
            VK_SHADER_STAGE_FRAGMENT_BIT,
            3 * 4,
            32 * 4
    );
    ranges.push_back(frag_range);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo =
        initializers::pipeline_layout_create_info(&descriptor_set_layout, 1);
    pipelineLayoutInfo.pushConstantRangeCount = static_cast<uint32_t>(ranges.size());
    pipelineLayoutInfo.pPushConstantRanges = ranges.data();
    VkPipelineLayout pipeline_layout;
    vkCreatePipelineLayout(device.device, &pipelineLayoutInfo, nullptr, &pipeline_layout);
    return pipeline_layout;
}

VkPipelineLayout create_pipeline_layout_bunny(Device& device) {
    std::vector<VkPushConstantRange> ranges;

    VkPushConstantRange vertex_range = initializers::push_constant_range(
            VK_SHADER_STAGE_VERTEX_BIT,
            32 * 4, //mat4 should™ have this size
            0
    );
    ranges.push_back(vertex_range);

    VkPushConstantRange frag_range = initializers::push_constant_range(
            VK_SHADER_STAGE_FRAGMENT_BIT,
            3 * 4,
            32 * 4
    );
    ranges.push_back(frag_range);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo =
        initializers::pipeline_layout_create_info(nullptr, 0);
    pipelineLayoutInfo.pushConstantRangeCount = static_cast<uint32_t>(ranges.size());
    pipelineLayoutInfo.pPushConstantRanges = ranges.data();
    VkPipelineLayout pipeline_layout;
    vkCreatePipelineLayout(device.device, &pipelineLayoutInfo, nullptr, &pipeline_layout);
    return pipeline_layout;
}

VkPipelineLayout create_pipeline_layout(Device& device) {
    std::vector<VkPushConstantRange> ranges;

#ifdef LAYOUT_REFLECTION
    for (auto& layout : shader_pipeline->reflected_layouts) {
        ranges.insert(ranges.end(), layout.push_constants.begin(), layout.push_constants.end());
    }
#else
    VkPushConstantRange tess_control_range = initializers::push_constant_range(
            VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
            17 * 4, //mat4 should™ have this size
            16 * 4
    );
    ranges.push_back(tess_control_range);

    VkPushConstantRange tess_eval_range = initializers::push_constant_range(
            VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
            16 * 4, //mat4 should™ have this size
            0
    );
    ranges.push_back(tess_eval_range);

    VkPushConstantRange frag_range = initializers::push_constant_range(
            VK_SHADER_STAGE_FRAGMENT_BIT,
            3 * 4,
            33 * 4
    );
    ranges.push_back(frag_range);
#endif

    VkPipelineLayoutCreateInfo pipelineLayoutInfo =
        initializers::pipeline_layout_create_info(nullptr, 0);
    pipelineLayoutInfo.pushConstantRangeCount = static_cast<uint32_t>(ranges.size());
    pipelineLayoutInfo.pPushConstantRanges = ranges.data();

    VkPipelineLayout pipeline_layout;
    vkCreatePipelineLayout(device.device, &pipelineLayoutInfo, nullptr, &pipeline_layout);












    return pipeline_layout;
}

VkPipeline create_pipeline(Device& device, Swapchain& swapchain, VkPipelineLayout& pipeline_layout, std::vector<VkPipelineShaderStageCreateInfo> shader_stages, VkPolygonMode polygon_mode, bool has_tessellation, bool ccw) {
    VkPipelineInputAssemblyStateCreateInfo input_assembly_state =
        initializers::pipeline_input_assembly_state_create_info(
                has_tessellation ? VK_PRIMITIVE_TOPOLOGY_PATCH_LIST : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
                0,
                VK_FALSE);

    VkPipelineRasterizationStateCreateInfo rasterization_state =
        initializers::pipeline_rasterization_state_create_info(
                polygon_mode,
                VK_CULL_MODE_BACK_BIT,
                ccw ? VK_FRONT_FACE_COUNTER_CLOCKWISE : VK_FRONT_FACE_CLOCKWISE,
                0);

    VkPipelineColorBlendAttachmentState blend_attachment_state =
        initializers::pipeline_color_blend_attachment_state(
                0xf,
                VK_FALSE);

    const auto color_attachment_state = initializers::pipeline_color_blend_attachment_state(0xf, VK_FALSE);

    VkPipelineColorBlendStateCreateInfo color_blend_state =
        initializers::pipeline_color_blend_state_create_info(
                1,
                &blend_attachment_state);
    color_blend_state.attachmentCount = 1;
    color_blend_state.pAttachments    = &color_attachment_state;

    // Note: Using reversed depth-buffer for increased precision, so Greater depth values are kept
    VkPipelineDepthStencilStateCreateInfo depth_stencil_state =
        initializers::pipeline_depth_stencil_state_create_info(
                VK_TRUE,
                VK_TRUE,
                VK_COMPARE_OP_LESS);
    depth_stencil_state.depthBoundsTestEnable = VK_FALSE;
    depth_stencil_state.stencilTestEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewport_state =
        initializers::pipeline_viewport_state_create_info(1, 1, 0);

    VkPipelineMultisampleStateCreateInfo multisample_state =
        initializers::pipeline_multisample_state_create_info(
                VK_SAMPLE_COUNT_1_BIT,
                0);

    std::vector<VkDynamicState> dynamic_state_enables = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic_state =
        initializers::pipeline_dynamic_state_create_info(
                dynamic_state_enables.data(),
                static_cast<uint32_t>(dynamic_state_enables.size()),
                0);

    auto bindingDescription = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();

    VkPipelineVertexInputStateCreateInfo vertex_input_state = initializers::pipeline_vertex_input_state_create_info();
    vertex_input_state.vertexBindingDescriptionCount        = 1;
    vertex_input_state.pVertexBindingDescriptions           = &bindingDescription;
    vertex_input_state.vertexAttributeDescriptionCount      = static_cast<uint32_t>(attributeDescriptions.size());
    vertex_input_state.pVertexAttributeDescriptions         = attributeDescriptions.data();

    VkPipelineTessellationStateCreateInfo tessellation_state = initializers::pipeline_tessellation_state_create_info(3);

    // Create graphics pipeline for dynamic rendering
    VkFormat color_rendering_format = swapchain.format();
    VkFormat depth_format = swapchain.depth_format();

    // Provide information for dynamic rendering
    VkPipelineRenderingCreateInfoKHR pipeline_create{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR};
    pipeline_create.pNext                   = VK_NULL_HANDLE;
    pipeline_create.colorAttachmentCount    = 1;
    pipeline_create.pColorAttachmentFormats = &color_rendering_format;
    pipeline_create.depthAttachmentFormat   = depth_format;
    if (hasStencilComponent(depth_format))
    {
        pipeline_create.stencilAttachmentFormat = depth_format;
    }

    // Use the pNext to point to the rendering create struct
    VkGraphicsPipelineCreateInfo graphics_create{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    graphics_create.pNext               = &pipeline_create;
    graphics_create.renderPass          = VK_NULL_HANDLE;
    graphics_create.pInputAssemblyState = &input_assembly_state;
    graphics_create.pRasterizationState = &rasterization_state;
    graphics_create.pColorBlendState    = &color_blend_state;
    graphics_create.pMultisampleState   = &multisample_state;
    graphics_create.pViewportState      = &viewport_state;
    graphics_create.pDepthStencilState  = &depth_stencil_state;
    graphics_create.pDynamicState       = &dynamic_state;
    graphics_create.pVertexInputState   = &vertex_input_state;
    graphics_create.pTessellationState  = &tessellation_state;
    graphics_create.stageCount          = static_cast<uint32_t>(shader_stages.size());
    graphics_create.pStages             = shader_stages.data();
    graphics_create.layout              = pipeline_layout;

    VkPipeline graphicsPipeline;
    vkCreateGraphicsPipelines(device.device, VK_NULL_HANDLE, 1, &graphics_create, VK_NULL_HANDLE, &graphicsPipeline);
    return graphicsPipeline;
}

VkImageView create_image_view(Device& device, Image& image, VkImageAspectFlags aspectFlags) {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image.handle();
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = image.format();
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    VkImageView imageView;
    if (vkCreateImageView(device.device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
        throw std::runtime_error("failed to create image view!");
    }

    return imageView;
}
