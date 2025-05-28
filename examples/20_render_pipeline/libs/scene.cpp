#include "scene.h"
#include "model.h"
#include "inih/INIReader.h"
#include "tooling.h"
#include "base_objects.h"

#include <iostream>

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
    tess_factor = reader.GetReal("plane", "tessellation", 25.0f);

    {
        int plane_size = reader.GetInteger("plane", "grid_points", 0);

        std::vector<Vertex> vertex_data_cpu;
        create_flat_surface(vertex_data_cpu, plane_size);
        std::unique_ptr<imr::Buffer> vertex_data_buffer = std::make_unique<imr::Buffer>(device, sizeof(vertex_data_cpu[0]) * vertex_data_cpu.size(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        Vertex * vertex_data;
        CHECK_VK(vkMapMemory(device.device, vertex_data_buffer->memory, vertex_data_buffer->memory_offset, vertex_data_buffer->size, 0, (void**) &vertex_data), abort());
        memcpy(vertex_data, vertex_data_cpu.data(), sizeof(vertex_data_cpu[0]) * vertex_data_cpu.size());
        vkUnmapMemory(device.device, vertex_data_buffer->memory);

        tess_landscape = reader.GetBoolean("plane", "noise", false);
        if (tess_landscape) {
            std::vector<VkPipelineShaderStageCreateInfo> shader_stages = create_shader_stages(device, use_glsl);
            VkPipelineLayout pipeline_layout = create_pipeline_layout(device);
            VkPipeline graphics_pipeline_fill = create_pipeline(device, swapchain, pipeline_layout, shader_stages, VK_POLYGON_MODE_FILL, true, false);
            VkPipeline graphics_pipeline_grid = create_pipeline(device, swapchain, pipeline_layout, shader_stages, VK_POLYGON_MODE_LINE, true, false);

            render_steps.emplace_back(true, pipeline_layout, graphics_pipeline_fill, graphics_pipeline_grid, std::move(vertex_data_buffer), identity_mat4);
        } else {
            std::vector<VkPipelineShaderStageCreateInfo> shader_stages = create_shader_stages_bunny(device);
            VkPipelineLayout pipeline_layout = create_pipeline_layout_bunny(device);
            VkPipeline graphics_pipeline_fill = create_pipeline(device, swapchain, pipeline_layout, shader_stages, VK_POLYGON_MODE_FILL, false, false);
            VkPipeline graphics_pipeline_grid = create_pipeline(device, swapchain, pipeline_layout, shader_stages, VK_POLYGON_MODE_LINE, false, false);

            render_steps.emplace_back(false, pipeline_layout, graphics_pipeline_fill, graphics_pipeline_grid, std::move(vertex_data_buffer), identity_mat4);
        }
    }

    for (int i = 0;; i++) {
        std::string model_name = reader.Get("model", "models[" + std::to_string(i) + "]", "");
        if (model_name != "") {
            std::vector<VkPipelineShaderStageCreateInfo> shader_stages = create_shader_stages_bunny(device);
            VkPipelineLayout pipeline_layout = create_pipeline_layout_bunny(device);
            VkPipeline graphics_pipeline_fill = create_pipeline(device, swapchain, pipeline_layout, shader_stages, VK_POLYGON_MODE_FILL, false, true);
            VkPipeline graphics_pipeline_grid = create_pipeline(device, swapchain, pipeline_layout, shader_stages, VK_POLYGON_MODE_LINE, false, true);

            Model model((std::filesystem::path(filename).parent_path() / std::filesystem::path(reader.Get("model." + model_name, "model", ""))).c_str(), device);

            float rotation_x = reader.GetReal("model." + model_name, "rotation[x]", 0);
            float rotation_y = reader.GetReal("model." + model_name, "rotation[y]", 0);
            float rotation_z = reader.GetReal("model." + model_name, "rotation[z]", 0);

            float offset_x = reader.GetReal("model." + model_name, "offset[x]", 0);
            float offset_y = reader.GetReal("model." + model_name, "offset[y]", 0);
            float offset_z = reader.GetReal("model." + model_name, "offset[z]", 0);

            printf("%f %f %f\n", rotation_x, rotation_y, rotation_z);

            mat4 object_matrix = identity_mat4;

            object_matrix = mul_mat4(rotate_axis_mat4(0, rotation_x / 180 * M_PI), object_matrix);
            object_matrix = mul_mat4(rotate_axis_mat4(1, rotation_y / 180 * M_PI), object_matrix);
            object_matrix = mul_mat4(rotate_axis_mat4(2, rotation_z / 180 * M_PI), object_matrix);

            object_matrix = mul_mat4(translate_mat4(vec3(offset_x, offset_y, offset_z)), object_matrix);

            render_steps.emplace_back(false, pipeline_layout, graphics_pipeline_fill, graphics_pipeline_grid, std::move(model.triangles_gpu), object_matrix);
        } else {
            break;
        }
    }

}

void Scene::render_to_cmdbuf(VkCommandBuffer& cmdbuf, imr::Image& image) {
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

        vkCmdDraw(cmdbuf, static_cast<uint32_t>(render_step.vertex_buffer->size / sizeof(Vertex)), 1, 0, 0);
    }
}

Scene::~Scene() {
    for (auto& render_step : render_steps) {
        vkDestroyPipeline(device.device, render_step.pipeline_fill, nullptr);
        vkDestroyPipeline(device.device, render_step.pipeline_grid, nullptr);
    }
}
