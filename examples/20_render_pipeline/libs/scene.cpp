#include "scene.h"
#include "model.h"
#include "inih/INIReader.h"
#include "tooling.h"
#include "base_objects.h"

#include <iostream>

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

double Scene::get_timestep() {
    int runtime = 30;
    uint64_t nano_time = imr_get_time_nano() - start_time;
    double time_step = fmod((double) nano_time / 1000 / 1000 / 1000 / runtime, 1) - time_offset;
    return time_step;
}


void Scene::set_camera_to_timestep(imr::Image& image) {
    double time_step = get_timestep();

    auto camera_position = vec4(10 * sin(time_step * M_PI * 2), 0, 10 * cos(time_step * M_PI * 2), 1);
    //auto camera_rotation = vec4(cos(time_step * M_PI * 2), 0, sin(time_step * M_PI * 2), 1);

    //camera_position = rotate_axis_mat4(0, -0.15) * camera_position;
    camera_position = translate_mat4({0, -1.75, 0}) * camera_position;

    if (update_tess) {
        camera.position.x = camera_position.x;
        camera.position.y = camera_position.y;
        camera.position.z = camera_position.z;

        camera.rotation.pitch = 0.3;
        camera.rotation.yaw = M_PI / 2 - 0.5 - time_step * M_PI * 2;
    } else {
        Camera camera = {{0, 0, 0}, {0, 0}, 60};

        camera.position.x = camera_position.x;
        camera.position.y = camera_position.y;
        camera.position.z = camera_position.z;

        camera.rotation.pitch = 0.3;
        camera.rotation.yaw = M_PI / 2 - 0.5 - time_step * M_PI * 2;

        camera_control_matrix = camera_get_view_mat4(&camera, image.size().width, image.size().height);
    }
}


static std::unique_ptr<imr::Image> load_texture_image (imr::Device& device, std::filesystem::path filename) {
    auto& vk = device.dispatch;

    int texWidth, texHeight, texChannels;
    stbi_uc* uv_texture = stbi_load(filename.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    VkDeviceSize imageSize = texWidth * texHeight * 4;
    assert(uv_texture);

    imr::Buffer staging_buffer(device, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    void* stagingData;
    CHECK_VK(vkMapMemory(device.device, staging_buffer.memory, staging_buffer.memory_offset, staging_buffer.size, 0, (void**) &stagingData), abort());
    memcpy(stagingData, uv_texture, static_cast<size_t>(imageSize));
    vkUnmapMemory(device.device, staging_buffer.memory);

    stbi_image_free(uv_texture);

    VkExtent3D uv_image_extent = {(unsigned int)texWidth, (unsigned int)texHeight, 1};
    std::unique_ptr<imr::Image> uv_texture_image = std::make_unique<imr::Image>(device, VK_IMAGE_TYPE_2D, uv_image_extent, VK_FORMAT_R8G8B8A8_SRGB, (VkImageUsageFlagBits)(VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT));

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = device.pool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device.device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);


    vk.cmdPipelineBarrier2KHR(commandBuffer, tmpPtr((VkDependencyInfo) {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .dependencyFlags = 0,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = tmpPtr((VkImageMemoryBarrier2) {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
            .srcAccessMask = VK_ACCESS_2_NONE,
            .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .image = uv_texture_image->handle(),
            .subresourceRange = uv_texture_image->whole_image_subresource_range(),
        }),
    }));


    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;

    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;

    region.imageOffset = {0, 0, 0};
    region.imageExtent = {
        (unsigned int)texWidth,
        (unsigned int)texHeight,
        1
    };

    vkCmdCopyBufferToImage(
        commandBuffer,
        staging_buffer.handle,
        uv_texture_image->handle(),
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &region
    );


    vk.cmdPipelineBarrier2KHR(commandBuffer, tmpPtr((VkDependencyInfo) {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .dependencyFlags = 0,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = tmpPtr((VkImageMemoryBarrier2) {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            .dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .image = uv_texture_image->handle(),
            .subresourceRange = uv_texture_image->whole_image_subresource_range(),
        }),
    }));


    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(device.main_queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(device.main_queue);

    vkFreeCommandBuffers(device.device, device.pool, 1, &commandBuffer);

    return uv_texture_image;
}

Scene::Scene(const char* filename, imr::Device& device, imr::Swapchain& swapchain, bool use_glsl) : device(device) {
    INIReader reader(filename);
    if (reader.ParseError() < 0) {
        std::cerr << "Could not read scene file!\n";
        abort();
    }

    camera = {{0, 0, 0}, {0, 0}, 60};
    render_mode = FILL;
    flight = false;

    camera_state.fly_speed = reader.GetReal("scene", "speed", 1);

    fog_dropoff_lower = reader.GetReal("scene.fog", "lower", 1);
    fog_dropoff_upper = reader.GetReal("scene.fog", "upper", 1);
    fog_power = reader.GetInteger("scene.fog", "power", 1);

    {
        int plane_size = reader.GetInteger("plane", "grid_points", 0);

        if (plane_size > 0) {
            std::vector<Vertex> vertex_data_cpu;
            create_flat_surface(vertex_data_cpu, plane_size);
            std::unique_ptr<imr::Buffer> vertex_data_buffer = std::make_unique<imr::Buffer>(device, sizeof(vertex_data_cpu[0]) * vertex_data_cpu.size(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
            Vertex * vertex_data;
            CHECK_VK(vkMapMemory(device.device, vertex_data_buffer->memory, vertex_data_buffer->memory_offset, vertex_data_buffer->size, 0, (void**) &vertex_data), abort());
            memcpy(vertex_data, vertex_data_cpu.data(), sizeof(vertex_data_cpu[0]) * vertex_data_cpu.size());
            vkUnmapMemory(device.device, vertex_data_buffer->memory);

            tess_landscape = reader.GetBoolean("plane", "noise", false);
            tess_factor = reader.GetReal("plane", "tessellation", 25.0f);

            if (tess_landscape) {
                std::unique_ptr<MultiStagePipeline> shader_stages = create_shader_stages(device, use_glsl);

                VkPipelineLayout pipeline_layout = create_pipeline_layout(device);
                VkPipeline graphics_pipeline_fill = create_pipeline(device, swapchain, pipeline_layout, shader_stages->shader_create_info, VK_POLYGON_MODE_FILL, true, false);
                VkPipeline graphics_pipeline_grid = create_pipeline(device, swapchain, pipeline_layout, shader_stages->shader_create_info, VK_POLYGON_MODE_LINE, true, false);

                render_steps.emplace_back(true, pipeline_layout, graphics_pipeline_fill, graphics_pipeline_grid, std::move(vertex_data_buffer), identity_mat4);
            } else {
                std::unique_ptr<MultiStagePipeline> shader_stages = create_shader_stages_bunny(device);

                VkPipelineLayout pipeline_layout = create_pipeline_layout_bunny(device);
                VkPipeline graphics_pipeline_fill = create_pipeline(device, swapchain, pipeline_layout, shader_stages->shader_create_info, VK_POLYGON_MODE_FILL, false, false);
                VkPipeline graphics_pipeline_grid = create_pipeline(device, swapchain, pipeline_layout, shader_stages->shader_create_info, VK_POLYGON_MODE_LINE, false, false);

                render_steps.emplace_back(false, pipeline_layout, graphics_pipeline_fill, graphics_pipeline_grid, std::move(vertex_data_buffer), identity_mat4);
            }
        }
    }

    for (int i = 0;; i++) {
        std::string model_name = reader.Get("model", "models[" + std::to_string(i) + "]", "");
        if (model_name != "") {
            float scale_x = reader.GetReal("model." + model_name, "scale[x]", 1);
            float scale_y = reader.GetReal("model." + model_name, "scale[y]", 1);
            float scale_z = reader.GetReal("model." + model_name, "scale[z]", 1);

            float rotation_x = reader.GetReal("model." + model_name, "rotation[x]", 0);
            float rotation_y = reader.GetReal("model." + model_name, "rotation[y]", 0);
            float rotation_z = reader.GetReal("model." + model_name, "rotation[z]", 0);

            float offset_x = reader.GetReal("model." + model_name, "offset[x]", 0);
            float offset_y = reader.GetReal("model." + model_name, "offset[y]", 0);
            float offset_z = reader.GetReal("model." + model_name, "offset[z]", 0);

            std::string texture_name = reader.Get("model." + model_name, "texture", "");


            mat4 object_matrix = identity_mat4;

            mat4 scale_mat4 = {
                scale_x, 0, 0, 0,
                0, scale_y, 0, 0,
                0, 0, scale_z, 0,
                0, 0, 0, 1,
            };

            object_matrix = mul_mat4(scale_mat4, object_matrix);

            object_matrix = mul_mat4(rotate_axis_mat4(0, rotation_x / 180 * M_PI), object_matrix);
            object_matrix = mul_mat4(rotate_axis_mat4(1, rotation_y / 180 * M_PI), object_matrix);
            object_matrix = mul_mat4(rotate_axis_mat4(2, rotation_z / 180 * M_PI), object_matrix);

            object_matrix = mul_mat4(translate_mat4(vec3(offset_x, offset_y, offset_z)), object_matrix);

            Model model((std::filesystem::path(filename).parent_path() / std::filesystem::path(reader.Get("model." + model_name, "model", ""))).c_str(), device);

            if (texture_name != "") {
                std::unique_ptr<imr::Image> uv_texture_image = load_texture_image(device,
                    std::filesystem::path(filename).parent_path() / std::filesystem::path(reader.Get("model." + model_name, "texture", ""))
                );
                VkImageView texture_image_view = create_image_view(device, *uv_texture_image, VK_IMAGE_ASPECT_COLOR_BIT);

                VkDescriptorSetLayoutBinding samplerLayoutBinding = initializers::descriptor_set_layout_binding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0, 1);
                VkDescriptorSetLayoutCreateInfo samplerLayoutCreateInfo = initializers::descriptor_set_layout_create_info(&samplerLayoutBinding, 1);
                VkDescriptorSetLayout descriptor_set_layout;
                vkCreateDescriptorSetLayout(device.device, &samplerLayoutCreateInfo, nullptr, &descriptor_set_layout);

                VkSamplerCreateInfo samplerInfo = initializers::sampler_create_info();
                samplerInfo.magFilter = VK_FILTER_LINEAR;
                samplerInfo.minFilter = VK_FILTER_LINEAR;
                samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
                samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
                samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
                samplerInfo.anisotropyEnable = VK_TRUE;
                samplerInfo.maxAnisotropy = 16;
                samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
                samplerInfo.unnormalizedCoordinates = VK_FALSE;
                samplerInfo.compareEnable = VK_FALSE;
                samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
                samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
                samplerInfo.mipLodBias = 0.0f;
                samplerInfo.minLod = 0.0f;
                samplerInfo.maxLod = 0.0f;
                VkSampler texture_sampler;
                vkCreateSampler(device.device, &samplerInfo, nullptr, &texture_sampler);

                VkDescriptorPoolSize poolSize = initializers::descriptor_pool_size(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1);
                VkDescriptorPoolCreateInfo poolInfo = initializers::descriptor_pool_create_info(1, &poolSize, 1);
                VkDescriptorPool descriptorPool;
                vkCreateDescriptorPool(device.device, &poolInfo, nullptr, &descriptorPool);

                VkDescriptorSetAllocateInfo dallocInfo = initializers::descriptor_set_allocate_info(descriptorPool, &descriptor_set_layout, 1);
                VkDescriptorSet descriptorSet;
                vkAllocateDescriptorSets(device.device, &dallocInfo, &descriptorSet);

                VkDescriptorImageInfo imageInfo = initializers::descriptor_image_info(texture_sampler, texture_image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                VkWriteDescriptorSet descriptorWrite = initializers::write_descriptor_set(descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &imageInfo);
                vkUpdateDescriptorSets(device.device, 1, &descriptorWrite, 0, nullptr);

                std::unique_ptr<MultiStagePipeline> shader_stages = create_shader_stages_textured(device);

                VkPipelineLayout pipeline_layout = create_pipeline_layout_textured(device, descriptor_set_layout);
                VkPipeline graphics_pipeline_fill = create_pipeline(device, swapchain, pipeline_layout, shader_stages->shader_create_info, VK_POLYGON_MODE_FILL, false, true);
                VkPipeline graphics_pipeline_grid = create_pipeline(device, swapchain, pipeline_layout, shader_stages->shader_create_info, VK_POLYGON_MODE_LINE, false, true);

                render_steps.emplace_back(false, pipeline_layout, graphics_pipeline_fill, graphics_pipeline_grid, std::move(model.triangles_gpu), object_matrix, std::move(uv_texture_image), texture_image_view, descriptor_set_layout, descriptorPool, descriptorSet, texture_sampler);
            } else {
                std::unique_ptr<MultiStagePipeline> shader_stages = create_shader_stages_bunny(device);

                VkPipelineLayout pipeline_layout = create_pipeline_layout_bunny(device);
                VkPipeline graphics_pipeline_fill = create_pipeline(device, swapchain, pipeline_layout, shader_stages->shader_create_info, VK_POLYGON_MODE_FILL, false, true);
                VkPipeline graphics_pipeline_grid = create_pipeline(device, swapchain, pipeline_layout, shader_stages->shader_create_info, VK_POLYGON_MODE_LINE, false, true);

                render_steps.emplace_back(false, pipeline_layout, graphics_pipeline_fill, graphics_pipeline_grid, std::move(model.triangles_gpu), object_matrix);
            }

        } else {
            break;
        }
    }
}

void Scene::render_to_cmdbuf(VkCommandBuffer& cmdbuf, imr::Image& image, imr::Swapchain::Frame& frame) {
    if (flight)
        set_camera_to_timestep(image);

    mat4 camera_matrix = camera_get_view_mat4(&camera, image.size().width, image.size().height);

    if (update_tess)
        camera_control_matrix = camera_matrix;

    struct {
        int fog_power;
        float fog_dropoff_lower;
        float fog_dropoff_upper;
    } fog_constants { fog_power, fog_dropoff_lower, fog_dropoff_upper };

    for (auto& render_step : render_steps) {
        switch (render_mode) {
        case FILL: vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, render_step.pipeline_fill); break;
        case GRID: vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, render_step.pipeline_grid); break;
        default: vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, render_step.pipeline_fill); break;
        }

        if (render_step.is_tessellated) {
            vkCmdPushConstants(cmdbuf, render_step.pipeline_layout, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, 0, 4 * 16, &camera_matrix);
            vkCmdPushConstants(cmdbuf, render_step.pipeline_layout, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, 4 * 16, 4 * 16, &camera_control_matrix);
            vkCmdPushConstants(cmdbuf, render_step.pipeline_layout, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, 4 * 32, 4, &tess_factor);
            vkCmdPushConstants(cmdbuf, render_step.pipeline_layout, VK_SHADER_STAGE_FRAGMENT_BIT, 4 * 33, 3 * 4, &fog_constants);
        } else {
            vkCmdPushConstants(cmdbuf, render_step.pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, 4 * 16, &camera_matrix);
            vkCmdPushConstants(cmdbuf, render_step.pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 4 * 16, 4 * 16, &render_step.object_transformation);
            vkCmdPushConstants(cmdbuf, render_step.pipeline_layout, VK_SHADER_STAGE_FRAGMENT_BIT, 4 * 32, 3 * 4, &fog_constants);
        }

        VkBuffer vertexBuffers2[] = {render_step.vertex_buffer->handle};
        VkDeviceSize offsets2[] = {0};
        vkCmdBindVertexBuffers(cmdbuf, 0, 1, vertexBuffers2, offsets2);

        if (render_step.texture_sampler)
            vkCmdBindDescriptorSets(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, render_step.pipeline_layout, 0, 1, &render_step.descriptorSet, 0, nullptr);

        vkCmdDraw(cmdbuf, static_cast<uint32_t>(render_step.vertex_buffer->size / sizeof(Vertex)), 1, 0, 0);
    }
}

Scene::~Scene() {
    for (auto& render_step : render_steps) {
        vkDestroyPipeline(device.device, render_step.pipeline_fill, nullptr);
        vkDestroyPipeline(device.device, render_step.pipeline_grid, nullptr);

        vkDestroyPipelineLayout(device.device, render_step.pipeline_layout, nullptr);


        if (render_step.texture_sampler) {
            vkDestroySampler(device.device, render_step.texture_sampler, nullptr);
            vkDestroyImageView(device.device, render_step.texture_image_view, nullptr);
            vkDestroyDescriptorPool(device.device, render_step.descriptorPool, nullptr);
            vkDestroyDescriptorSetLayout(device.device, render_step.descriptor_set_layout, nullptr);
        }
    }
}
