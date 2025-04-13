#include "imr/imr.h"
#include "imr/util.h"

#include <fstream>

#include "VkBootstrap.h"

#include "initializers.h"

#include "libs/camera.h"

Camera camera;
CameraFreelookState camera_state = {
    .fly_speed = 1.0f,
    .mouse_sensitivity = 1,
};
CameraInput camera_input;

static auto time() -> uint64_t {
    timespec t = { 0 };
    timespec_get(&t, TIME_UTC);
    return t.tv_sec * 1000000000 + t.tv_nsec;
}

void camera_update(GLFWwindow*, CameraInput* input);

static VkShaderModule createShaderModule(imr::Device& device, const std::vector<char>& code) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data()); //This requires the code to be int aligned, not char aligned!

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device.device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
        throw std::runtime_error("failed to create shader module!");
    return shaderModule;
}

static std::vector<char> readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("failed to open file!");
    }
    size_t fileSize = (size_t) file.tellg();
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);

    file.close();

    return buffer;
}

bool is_depth_only_format(VkFormat format)
{
	return format == VK_FORMAT_D16_UNORM ||
	       format == VK_FORMAT_D32_SFLOAT;
}

static VkFormat get_suitable_depth_format(VkPhysicalDevice physical_device, bool depth_only, const std::vector<VkFormat> &depth_format_priority_list)
{
	VkFormat depth_format{VK_FORMAT_UNDEFINED};

	for (auto &format : depth_format_priority_list)
	{
		if (depth_only && !is_depth_only_format(format))
		{
			continue;
		}

		VkFormatProperties properties;
		vkGetPhysicalDeviceFormatProperties(physical_device, format, &properties);

		// Format must support depth stencil attachment for optimal tiling
		if (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
		{
			depth_format = format;
			break;
		}
	}

        if (depth_format != VK_FORMAT_UNDEFINED) {
            return depth_format;
        }

	throw std::runtime_error("No suitable depth format could be determined");
}

VkPipelineShaderStageCreateInfo load_shader(imr::Device& device, const std::string& filename, VkShaderStageFlagBits stage_bits) {
    auto shaderCode = readFile(filename);
    VkShaderModule shaderModule = createShaderModule(device, shaderCode);

    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = stage_bits;
    vertShaderStageInfo.module = shaderModule;
    vertShaderStageInfo.pName = "main";
    return vertShaderStageInfo;
}

VkPipelineLayout create_pipeline_layout(imr::Device& device) {
    VkPushConstantRange range = initializers::push_constant_range(
            VK_SHADER_STAGE_VERTEX_BIT,
            16 * 4, //mat4 should™ have this size
            0
    );

    VkPipelineLayoutCreateInfo pipelineLayoutInfo =
        initializers::pipeline_layout_create_info(nullptr, 0);
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &range;
    VkPipelineLayout pipeline_layout;
    vkCreatePipelineLayout(device.device, &pipelineLayoutInfo, nullptr, &pipeline_layout);
    return pipeline_layout;
}

VkPipeline create_pipeline(imr::Device& device, imr::Swapchain& swapchain, VkPipelineLayout& pipeline_layout) {
    auto depth_format = get_suitable_depth_format(device.physical_device, false, 
        {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D16_UNORM});

    VkPipelineInputAssemblyStateCreateInfo input_assembly_state =
        initializers::pipeline_input_assembly_state_create_info(
                VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
                0,
                VK_FALSE);

    VkPipelineRasterizationStateCreateInfo rasterization_state =
        initializers::pipeline_rasterization_state_create_info(
                VK_POLYGON_MODE_FILL,
                VK_CULL_MODE_BACK_BIT,
                VK_FRONT_FACE_CLOCKWISE,
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
                VK_FALSE,
                VK_FALSE,
                VK_COMPARE_OP_GREATER);

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

    VkPipelineVertexInputStateCreateInfo vertex_input_state = initializers::pipeline_vertex_input_state_create_info();
    vertex_input_state.vertexBindingDescriptionCount        = 0;
    vertex_input_state.pVertexBindingDescriptions           = nullptr;
    vertex_input_state.vertexAttributeDescriptionCount      = 0;
    vertex_input_state.pVertexAttributeDescriptions         = nullptr;

    std::array<VkPipelineShaderStageCreateInfo, 2> shader_stages{};
    shader_stages[0] = load_shader(device, "shaders/shader.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shader_stages[1] = load_shader(device, "shaders/shader.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

    // Create graphics pipeline for dynamic rendering
    VkFormat color_rendering_format = swapchain.format();

    // Provide information for dynamic rendering
    VkPipelineRenderingCreateInfoKHR pipeline_create{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR};
    pipeline_create.pNext                   = VK_NULL_HANDLE;
    pipeline_create.colorAttachmentCount    = 1;
    pipeline_create.pColorAttachmentFormats = &color_rendering_format;
    pipeline_create.depthAttachmentFormat   = depth_format;
    if (!is_depth_only_format(depth_format))
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
    graphics_create.stageCount          = static_cast<uint32_t>(shader_stages.size());
    graphics_create.pStages             = shader_stages.data();
    graphics_create.layout              = pipeline_layout;

    VkPipeline graphicsPipeline;
    vkCreateGraphicsPipelines(device.device, VK_NULL_HANDLE, 1, &graphics_create, VK_NULL_HANDLE, &graphicsPipeline);
    return graphicsPipeline;
}

int main() {
    camera = {{0, 0, 0}, {0, 0}, 60};

    glfwInit();
    glfwWindowHint(GLFW_RESIZABLE, GL_TRUE);
    glfwWindowHintString(GLFW_X11_CLASS_NAME, "vcc_demo");
    glfwWindowHintString(GLFW_X11_INSTANCE_NAME, "vcc_demo");
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    auto window = glfwCreateWindow(1024, 1024, "Example", nullptr, nullptr);

    imr::Context context;
    imr::Device device(context);
    imr::Swapchain swapchain(device, window);
    imr::FpsCounter fps_counter;

    glfwSetKeyCallback(window, [](GLFWwindow* window, int key, int scancode, int action, int mods) {
        if (action == GLFW_PRESS && key == GLFW_KEY_F4) {
            printf("--position %f %f %f --rotation %f %f --fov %f\n", (float) camera.position.x, (float) camera.position.y, (float) camera.position.z, (float) camera.rotation.yaw, (float) camera.rotation.pitch, (float) camera.fov);
        }
        if (action == GLFW_PRESS && key == GLFW_KEY_MINUS) {
            camera.fov -= 0.02f;
        }
        if (action == GLFW_PRESS && key == GLFW_KEY_EQUAL) {
            camera.fov += 0.02f;
        }
    });

    auto vkCmdBeginRenderingKHR = reinterpret_cast<PFN_vkCmdBeginRenderingKHR>(vkGetInstanceProcAddr(context.instance, "vkCmdBeginRenderingKHR"));
    auto vkCmdEndRenderingKHR   = reinterpret_cast<PFN_vkCmdEndRenderingKHR>(vkGetInstanceProcAddr(context.instance, "vkCmdEndRenderingKHR"));

    auto& vk = device.dispatch;

    VkPipelineLayout pipeline_layout = create_pipeline_layout(device);
    VkPipeline graphics_pipeline = create_pipeline(device, swapchain, pipeline_layout);

    auto epoch = time();
    auto prev_frame = epoch;
    float delta = 0;

    while (!glfwWindowShouldClose(window)) {
        swapchain.beginFrame([&](auto& frame) {
            camera_update(window, &camera_input);
            camera_move_freelook(&camera, &camera_input, &camera_state, delta);


            auto& image = frame.swapchain_image;

            VkCommandBuffer cmdbuf;
            vkAllocateCommandBuffers(device.device, tmp((VkCommandBufferAllocateInfo) {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .commandPool = device.pool,
                .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount = 1,
            }), &cmdbuf);

            vkBeginCommandBuffer(cmdbuf, tmp((VkCommandBufferBeginInfo) {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            }));

            VkSemaphore sem;
            CHECK_VK(vkCreateSemaphore(device.device, tmp((VkSemaphoreCreateInfo) {
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            }), nullptr, &sem), abort());

            vk.setDebugUtilsObjectNameEXT(tmp((VkDebugUtilsObjectNameInfoEXT) {
                .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
                .objectType = VK_OBJECT_TYPE_SEMAPHORE,
                .objectHandle = reinterpret_cast<uint64_t>(sem),
                .pObjectName = (std::string("12_render_pipeline.sem") + std::to_string(frame.id)).c_str(),
            }));

            mat4 camera_matrix = camera_get_view_mat4(&camera, frame.width, frame.height);
            vkCmdPushConstants(cmdbuf, pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, 4 * 16, &camera_matrix);

            vk.cmdPipelineBarrier2KHR(cmdbuf, tmp((VkDependencyInfo) {
                .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .dependencyFlags = 0,
                .imageMemoryBarrierCount = 1,
                .pImageMemoryBarriers = tmp((VkImageMemoryBarrier2) {
                    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                    .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
                    .srcAccessMask = VK_ACCESS_2_NONE,
                    .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                    .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                    .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                    .newLayout = VK_IMAGE_LAYOUT_GENERAL,
                    .image = image,
                    .subresourceRange = {
                        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                        .levelCount = 1,
                        .layerCount = 1,
                    }
                }),
            }));

            VkImageViewCreateInfo imageViewCreateInfo = initializers::image_view_create_info();
            imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            imageViewCreateInfo.format = swapchain.format();
            imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
            imageViewCreateInfo.subresourceRange.levelCount = 1;
            imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
            imageViewCreateInfo.subresourceRange.layerCount = 1;
            imageViewCreateInfo.image = image;

            VkImageView imageView;
            vkCreateImageView(device.device, &imageViewCreateInfo, nullptr, &imageView);

            VkRenderingAttachmentInfoKHR color_attachment_info = initializers::rendering_attachment_info();
            color_attachment_info.imageView = imageView;
            color_attachment_info.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR;
            color_attachment_info.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            color_attachment_info.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            color_attachment_info.clearValue = {{{ 0.0f, 0.0f, 0.0f, 1.0f}}};

            VkRenderingInfoKHR render_info = initializers::rendering_info(
                initializers::rect2D(static_cast<int>(frame.width), static_cast<int>(frame.height), 0, 0),
                1,
                &color_attachment_info
            );
            render_info.layerCount = 1,

            vkCmdBeginRenderingKHR(cmdbuf, &render_info);

            VkViewport viewport = initializers::viewport(static_cast<float>(frame.width), static_cast<float>(frame.height), 0.0f, 1.0f);
            vkCmdSetViewport(cmdbuf, 0, 1, &viewport);

            VkRect2D scissor = initializers::rect2D(static_cast<int>(frame.width), static_cast<int>(frame.height), 0, 0);
            vkCmdSetScissor(cmdbuf, 0, 1, &scissor);

            vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline);
            vkCmdDraw(cmdbuf, 3, 1, 0, 0);

            vkCmdEndRenderingKHR(cmdbuf);

            vk.cmdPipelineBarrier2KHR(cmdbuf, tmp((VkDependencyInfo) {
                .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .dependencyFlags = 0,
                .imageMemoryBarrierCount = 1,
                .pImageMemoryBarriers = tmp((VkImageMemoryBarrier2) {
                    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                    .srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT,
                    .srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT,
                    .dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                    .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
                    .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
                    .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                    .image = image,
                    .subresourceRange = {
                        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                        .levelCount = 1,
                        .layerCount = 1,
                    }
                }),
            }));

            VkFence fence;
            vkCreateFence(device.device, tmp((VkFenceCreateInfo) {
                .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                .flags = 0,
            }), nullptr, &fence);

            vkEndCommandBuffer(cmdbuf);
            vkQueueSubmit(device.main_queue, 1, tmp((VkSubmitInfo) {
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                .waitSemaphoreCount = 1,
                .pWaitSemaphores = &frame.swapchain_image_available,
                .pWaitDstStageMask = tmp((VkPipelineStageFlags) VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT),
                .commandBufferCount = 1,
                .pCommandBuffers = &cmdbuf,
                .signalSemaphoreCount = 1,
                .pSignalSemaphores = &sem,
            }), fence);

            //printf("Frame submitted with fence = %llx\n", fence);

            frame.add_to_delete_queue(fence, [=, &device]() {
                //vkWaitForFences(context.device, 1, &fence, true, UINT64_MAX);
                vkDestroyFence(device.device, fence, nullptr);
                vkDestroySemaphore(device.device, sem, nullptr);
                vkDestroyImageView(device.device, imageView, nullptr);
                vkFreeCommandBuffers(device.device, device.pool, 1, &cmdbuf);
            });
            frame.present(sem);

            auto now = time();
            delta = (float) ((now - prev_frame) / 1000000) / 1000.0f;
            prev_frame = now;
        });

        fps_counter.tick();
        fps_counter.updateGlfwWindowTitle(window);
        glfwPollEvents();
    }

    vkDeviceWaitIdle(device.device);

    vkDestroyPipeline(device.device, graphics_pipeline, nullptr);

    return 0;
}
