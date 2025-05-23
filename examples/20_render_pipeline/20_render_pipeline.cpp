#include "imr/imr.h"
#include "imr/util.h"

#include <fstream>
#include <filesystem>

#include "VkBootstrap.h"

#include "initializers.h"
#include "tooling.h"

#include "libs/camera.h"
#include "libs/model.h"

Camera camera;
CameraFreelookState camera_state = {
    .fly_speed = 1.0f,
    .mouse_sensitivity = 1,
};
CameraInput camera_input;
float fog_dropoff_lower;
float fog_dropoff_upper;
int fog_power;
float fog_lower_old;
float fog_upper_old;
int fog_power_old;

float tess_factor;
bool update_tess = true;

RENDER_MODE render_mode;

bool flight;
uint64_t start_time = imr_get_time_nano();
double time_offset = 0;

double get_timestep() {
    int runtime = 30;
    uint64_t nano_time = imr_get_time_nano() - start_time;
    double time_step = fmod((double) nano_time / 1000 / 1000 / 1000 / runtime, 1) - time_offset;
    return time_step;
}

mat4 camera_control_matrix;

void set_camera_to_timestep(imr::Image& image) {
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

void camera_update(GLFWwindow*, CameraInput* input);

const vec3 vertex_1_color = {1.0f, 0.0f, 0.0f};
const vec3 vertex_2_color = {0.0f, 1.0f, 0.0f};
const vec3 vertex_3_color = {1.0f, 1.0f, 0.0f};
const vec3 vertex_4_color = {0.0f, 0.0f, 1.0f};
const vec3 vertex_5_color = {1.0f, 0.0f, 1.0f};
const vec3 vertex_6_color = {0.0f, 1.0f, 1.0f};
const vec3 vertex_7_color = {1.0f, 1.0f, 1.0f};

const std::vector<Vertex> vertices = {
    //face 1
    {{-0.5f, -0.5f,  0.5f}, {0, 0, 0}, vertex_1_color},
    {{ 0.5f,  0.5f,  0.5f}, {0, 0, 0}, vertex_2_color},
    {{-0.5f,  0.5f,  0.5f}, {0, 0, 0}, vertex_3_color},

    {{-0.5f, -0.5f,  0.5f}, {0, 0, 0}, vertex_1_color},
    {{ 0.5f, -0.5f,  0.5f}, {0, 0, 0}, vertex_2_color},
    {{ 0.5f,  0.5f,  0.5f}, {0, 0, 0}, vertex_3_color},

    //face 2
    {{ 0.5f,  0.5f, -0.5f}, {0, 0, 0}, vertex_1_color},
    {{-0.5f, -0.5f, -0.5f}, {0, 0, 0}, vertex_2_color},
    {{-0.5f,  0.5f, -0.5f}, {0, 0, 0}, vertex_3_color},

    {{ 0.5f, -0.5f, -0.5f}, {0, 0, 0}, vertex_1_color},
    {{-0.5f, -0.5f, -0.5f}, {0, 0, 0}, vertex_2_color},
    {{ 0.5f,  0.5f, -0.5f}, {0, 0, 0}, vertex_3_color},

    //face 3
    {{ 0.5f,  0.5f,  0.5f}, {0, 0, 0}, vertex_1_color},
    {{-0.5f,  0.5f, -0.5f}, {0, 0, 0}, vertex_2_color},
    {{-0.5f,  0.5f,  0.5f}, {0, 0, 0}, vertex_3_color},

    {{ 0.5f,  0.5f, -0.5f}, {0, 0, 0}, vertex_1_color},
    {{-0.5f,  0.5f, -0.5f}, {0, 0, 0}, vertex_2_color},
    {{ 0.5f,  0.5f,  0.5f}, {0, 0, 0}, vertex_3_color},

    //face 4
    {{ 0.5f, -0.5f,  0.5f}, {0, 0, 0}, vertex_1_color},
    {{-0.5f, -0.5f,  0.5f}, {0, 0, 0}, vertex_2_color},
    {{-0.5f, -0.5f, -0.5f}, {0, 0, 0}, vertex_3_color},

    {{ 0.5f, -0.5f, -0.5f}, {0, 0, 0}, vertex_1_color},
    {{ 0.5f, -0.5f,  0.5f}, {0, 0, 0}, vertex_2_color},
    {{-0.5f, -0.5f, -0.5f}, {0, 0, 0}, vertex_3_color},

    //face 5
    {{ 0.5f,  0.5f,  0.5f}, {0, 0, 0}, vertex_1_color},
    {{ 0.5f, -0.5f,  0.5f}, {0, 0, 0}, vertex_2_color},
    {{ 0.5f, -0.5f, -0.5f}, {0, 0, 0}, vertex_3_color},

    {{ 0.5f,  0.5f, -0.5f}, {0, 0, 0}, vertex_1_color},
    {{ 0.5f,  0.5f,  0.5f}, {0, 0, 0}, vertex_2_color},
    {{ 0.5f, -0.5f, -0.5f}, {0, 0, 0}, vertex_3_color},

    //face 6
    {{-0.5f,  0.5f,  0.5f}, {0, 0, 0}, vertex_1_color},
    {{-0.5f, -0.5f, -0.5f}, {0, 0, 0}, vertex_2_color},
    {{-0.5f, -0.5f,  0.5f}, {0, 0, 0}, vertex_3_color},

    {{-0.5f,  0.5f,  0.5f}, {0, 0, 0}, vertex_1_color},
    {{-0.5f,  0.5f, -0.5f}, {0, 0, 0}, vertex_2_color},
    {{-0.5f, -0.5f, -0.5f}, {0, 0, 0}, vertex_3_color},

    //face 1
    {{-0.5f, -0.5f,  0.5f-2.0f}, {0, 0, 0}, vertex_4_color},
    {{ 0.5f,  0.5f,  0.5f-2.0f}, {0, 0, 0}, vertex_5_color},
    {{-0.5f,  0.5f,  0.5f-2.0f}, {0, 0, 0}, vertex_6_color},

    {{-0.5f, -0.5f,  0.5f-2.0f}, {0, 0, 0}, vertex_4_color},
    {{ 0.5f, -0.5f,  0.5f-2.0f}, {0, 0, 0}, vertex_5_color},
    {{ 0.5f,  0.5f,  0.5f-2.0f}, {0, 0, 0}, vertex_6_color},

    //face 2
    {{ 0.5f,  0.5f, -0.5f-2.0f}, {0, 0, 0}, vertex_4_color},
    {{-0.5f, -0.5f, -0.5f-2.0f}, {0, 0, 0}, vertex_5_color},
    {{-0.5f,  0.5f, -0.5f-2.0f}, {0, 0, 0}, vertex_6_color},

    {{ 0.5f, -0.5f, -0.5f-2.0f}, {0, 0, 0}, vertex_4_color},
    {{-0.5f, -0.5f, -0.5f-2.0f}, {0, 0, 0}, vertex_5_color},
    {{ 0.5f,  0.5f, -0.5f-2.0f}, {0, 0, 0}, vertex_6_color},

    //face 3
    {{ 0.5f,  0.5f,  0.5f-2.0f}, {0, 0, 0}, vertex_4_color},
    {{-0.5f,  0.5f, -0.5f-2.0f}, {0, 0, 0}, vertex_5_color},
    {{-0.5f,  0.5f,  0.5f-2.0f}, {0, 0, 0}, vertex_6_color},

    {{ 0.5f,  0.5f, -0.5f-2.0f}, {0, 0, 0}, vertex_4_color},
    {{-0.5f,  0.5f, -0.5f-2.0f}, {0, 0, 0}, vertex_5_color},
    {{ 0.5f,  0.5f,  0.5f-2.0f}, {0, 0, 0}, vertex_6_color},

    //face 4
    {{ 0.5f, -0.5f,  0.5f-2.0f}, {0, 0, 0}, vertex_4_color},
    {{-0.5f, -0.5f,  0.5f-2.0f}, {0, 0, 0}, vertex_5_color},
    {{-0.5f, -0.5f, -0.5f-2.0f}, {0, 0, 0}, vertex_6_color},

    {{ 0.5f, -0.5f, -0.5f-2.0f}, {0, 0, 0}, vertex_4_color},
    {{ 0.5f, -0.5f,  0.5f-2.0f}, {0, 0, 0}, vertex_5_color},
    {{-0.5f, -0.5f, -0.5f-2.0f}, {0, 0, 0}, vertex_6_color},

    //face 5
    {{ 0.5f,  0.5f,  0.5f-2.0f}, {0, 0, 0}, vertex_4_color},
    {{ 0.5f, -0.5f,  0.5f-2.0f}, {0, 0, 0}, vertex_5_color},
    {{ 0.5f, -0.5f, -0.5f-2.0f}, {0, 0, 0}, vertex_6_color},

    {{ 0.5f,  0.5f, -0.5f-2.0f}, {0, 0, 0}, vertex_4_color},
    {{ 0.5f,  0.5f,  0.5f-2.0f}, {0, 0, 0}, vertex_5_color},
    {{ 0.5f, -0.5f, -0.5f-2.0f}, {0, 0, 0}, vertex_6_color},

    //face 6
    {{-0.5f,  0.5f,  0.5f-2.0f}, {0, 0, 0}, vertex_4_color},
    {{-0.5f, -0.5f, -0.5f-2.0f}, {0, 0, 0}, vertex_5_color},
    {{-0.5f, -0.5f,  0.5f-2.0f}, {0, 0, 0}, vertex_6_color},

    {{-0.5f,  0.5f,  0.5f-2.0f}, {0, 0, 0}, vertex_4_color},
    {{-0.5f,  0.5f, -0.5f-2.0f}, {0, 0, 0}, vertex_5_color},
    {{-0.5f, -0.5f, -0.5f-2.0f}, {0, 0, 0}, vertex_6_color},
};

static int TESSELATION = 20;

void create_flat_surface(std::vector<Vertex> & data) {
    //float GRID_SIZE = 1.0f / TESSELATION;
    float GRID_SIZE = 1.0f;
    for (int xi = -TESSELATION ; xi < TESSELATION; xi++) {
        for (int zi = -TESSELATION ; zi < TESSELATION; zi++) {
            Vertex a = {{(xi + 1) * GRID_SIZE,  0.f, (zi + 1) * GRID_SIZE}, {0, 0, 0}, vertex_2_color};
            Vertex b = {{     xi  * GRID_SIZE,  0.f, (zi + 1) * GRID_SIZE}, {0, 0, 0}, vertex_2_color};
            Vertex c = {{(xi + 1) * GRID_SIZE,  0.f,      zi  * GRID_SIZE}, {0, 0, 0}, vertex_2_color};
            Vertex d = {{     xi  * GRID_SIZE,  0.f,      zi  * GRID_SIZE}, {0, 0, 0}, vertex_2_color};
            data.push_back(a);
            data.push_back(b);
            data.push_back(d);
            data.push_back(a);
            data.push_back(d);
            data.push_back(c);
        }
    }
}

struct CommandArguments {
    bool use_glsl = true;
    std::optional<float> camera_speed;
    std::optional<vec3> camera_eye;
    std::optional<vec2> camera_rotation;
    std::optional<float> camera_fov;
};

int main(int argc, char ** argv) {
    char * model_filename = nullptr;
    CommandArguments cmd_args;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--speed") == 0) {
            cmd_args.camera_speed = strtof(argv[++i], nullptr);
            continue;
        }
        if (strcmp(argv[i], "--position") == 0) {
            vec3 pos;
            pos.x = strtof(argv[++i], nullptr);
            pos.y = strtof(argv[++i], nullptr);
            pos.z = strtof(argv[++i], nullptr);
            cmd_args.camera_eye = pos;
            continue;
        }
        if (strcmp(argv[i], "--rotation") == 0) {
            vec2 rot;
            rot.x = strtof(argv[++i], nullptr);
            rot.y = strtof(argv[++i], nullptr);
            cmd_args.camera_rotation = rot;
            continue;
        }
        if (strcmp(argv[i], "--fov") == 0) {
            vec2 rot;
            cmd_args.camera_fov = strtof(argv[++i], nullptr);
            continue;
        }
        if (strcmp(argv[i], "--glsl") == 0) {
            cmd_args.use_glsl = true;
            continue;
        }
        if (strcmp(argv[i], "--spv") == 0) {
            cmd_args.use_glsl = false;
            continue;
        }
        //model_filename = argv[i];
    }

    /*if (!model_filename) {
        printf("Usage: ./ra <model>\n");
        exit(-1);
    }*/

    glfwInit();
    glfwWindowHint(GLFW_RESIZABLE, GL_TRUE);
    glfwWindowHintString(GLFW_X11_CLASS_NAME, "vcc_demo");
    glfwWindowHintString(GLFW_X11_INSTANCE_NAME, "vcc_demo");
    glfwWindowHintString(GLFW_WAYLAND_APP_ID, "vcc_demo");
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    auto window = glfwCreateWindow(2048, 2048, "Example", nullptr, nullptr);

    imr::Context context;
    imr::Device device(context);
    imr::Swapchain swapchain(device, window);
    imr::FpsCounter fps_counter;

    auto depth_format = swapchain.depth_format();

    Model bunny_model((std::filesystem::path(imr_get_executable_location()).parent_path().string() + "/../../../examples/20_render_pipeline/models/bunny.obj").c_str(), device);

    camera = {{0, 0, 0}, {0, 0}, 60};
    fog_dropoff_lower = 0.98;
    fog_dropoff_upper = 0.995;
    fog_power = 10;
    tess_factor = 50.0f;
    render_mode = FILL;
    flight = false;
    //camera = model.loaded_camera;
    camera.position = cmd_args.camera_eye.value_or(camera.position);
    if (cmd_args.camera_rotation.has_value()) {
        camera.rotation.yaw = cmd_args.camera_rotation.value().x;
        camera.rotation.pitch = cmd_args.camera_rotation.value().y;
    }
    camera.fov = cmd_args.camera_fov.value_or(camera.fov);
    camera_state.fly_speed = cmd_args.camera_speed.value_or(camera_state.fly_speed);

    glfwSetKeyCallback(window, [](GLFWwindow* window, int key, int scancode, int action, int mods) {
        if (action == GLFW_PRESS && key == GLFW_KEY_F4) {
            printf("--position %f %f %f --rotation %f %f --fov %f\n", (float) camera.position.x, (float) camera.position.y, (float) camera.position.z, (float) camera.rotation.yaw, (float) camera.rotation.pitch, (float) camera.fov);
        }
        if ((action == GLFW_PRESS || action == GLFW_REPEAT) && key == GLFW_KEY_MINUS) {
            //camera.fov -= 2.0f;
            tess_factor -= int(tess_factor / 10) + 1;
            printf("Tesselation now %f\n", tess_factor);
        }
        if ((action == GLFW_PRESS || action == GLFW_REPEAT) && key == GLFW_KEY_EQUAL) {
            //camera.fov += 2.0f;
            tess_factor += int(tess_factor / 10) + 1;
            printf("Tesselation now %f\n", tess_factor);
        }
        if ((action == GLFW_PRESS || action == GLFW_REPEAT) && key == GLFW_KEY_1) {
            fog_power -= 1;
            printf("Fog power now %d\n", fog_power);
        }
        if ((action == GLFW_PRESS || action == GLFW_REPEAT) && key == GLFW_KEY_2) {
            fog_power += 1;
            printf("Fog power now %d\n", fog_power);
        }
        if ((action == GLFW_PRESS || action == GLFW_REPEAT) && key == GLFW_KEY_3) {
            fog_dropoff_lower -= (1 - fog_dropoff_lower) * 0.1f;
            printf("Fog lower now %f\n", fog_dropoff_lower);
        }
        if ((action == GLFW_PRESS || action == GLFW_REPEAT) && key == GLFW_KEY_4) {
            fog_dropoff_lower += (1 - fog_dropoff_lower) * 0.1f;
            printf("Fog lower now %f\n", fog_dropoff_lower);
        }
        if ((action == GLFW_PRESS || action == GLFW_REPEAT) && key == GLFW_KEY_5) {
            fog_dropoff_upper -= (1 - fog_dropoff_upper) * 0.1f;
            printf("Fog upper now %f\n", fog_dropoff_upper);
        }
        if ((action == GLFW_PRESS || action == GLFW_REPEAT) && key == GLFW_KEY_6) {
            fog_dropoff_upper += (1 - fog_dropoff_upper) * 0.1f;
            printf("Fog upper now %f\n", fog_dropoff_upper);
        }

        if (action == GLFW_PRESS && key == GLFW_KEY_F) {
            if (fog_dropoff_lower == 1.0f) {
                fog_dropoff_lower = fog_lower_old;
                fog_dropoff_upper = fog_upper_old;
                fog_power = fog_power_old;
            } else {
                fog_lower_old = fog_dropoff_lower;
                fog_upper_old = fog_dropoff_upper;
                fog_power_old = fog_power;

                fog_dropoff_lower = 1.0f;
                fog_dropoff_upper = 1.0f;
                fog_power = 1;
            }
        }

        if (action == GLFW_PRESS && key == GLFW_KEY_M) {
            switch (render_mode) {
            case FILL: render_mode = GRID; break;
            case GRID: render_mode = FILL; break;
            default: render_mode = FILL; break;
            }
        }

        if (action == GLFW_PRESS && key == GLFW_KEY_SPACE) {
            flight = flight ^ 1;

            time_offset = get_timestep();
        }

        if (action == GLFW_PRESS && key == GLFW_KEY_0) {
            update_tess = update_tess ^ 1;
            if (update_tess)
                printf("Tesselation cammera updating\n");
            else
                printf("Tesselation cammera stopped\n");
        }
    });

    auto vkCmdBeginRenderingKHR = reinterpret_cast<PFN_vkCmdBeginRenderingKHR>(vkGetInstanceProcAddr(context.instance, "vkCmdBeginRenderingKHR"));
    auto vkCmdEndRenderingKHR   = reinterpret_cast<PFN_vkCmdEndRenderingKHR>(vkGetInstanceProcAddr(context.instance, "vkCmdEndRenderingKHR"));

    auto& vk = device.dispatch;

    std::vector<Vertex> vertex_data_cpu;
    create_flat_surface(vertex_data_cpu);
    //auto vertex_data_cpu = vertices;

    std::unique_ptr<imr::Buffer> vertex_data_buffer = std::make_unique<imr::Buffer>(device, sizeof(vertex_data_cpu[0]) * vertex_data_cpu.size(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    Vertex * vertex_data;
    CHECK_VK(vkMapMemory(device.device, vertex_data_buffer->memory, vertex_data_buffer->memory_offset, vertex_data_buffer->size, 0, (void**) &vertex_data), abort());
    memcpy(vertex_data, vertex_data_cpu.data(), sizeof(vertex_data_cpu[0]) * vertex_data_cpu.size());
    vkUnmapMemory(device.device, vertex_data_buffer->memory);

    std::vector<VkPipelineShaderStageCreateInfo> shader_stages = create_shader_stages(device, cmd_args.use_glsl);
    VkPipelineLayout pipeline_layout = create_pipeline_layout(device);

    std::vector<VkPipelineShaderStageCreateInfo> shader_stages_bunny = create_shader_stages_bunny(device);
    VkPipelineLayout pipeline_layout_bunny = create_pipeline_layout_bunny(device);

    VkPipeline graphics_pipeline_fill = create_pipeline(device, swapchain, pipeline_layout, shader_stages, VK_POLYGON_MODE_FILL, true);
    VkPipeline graphics_pipeline_grid = create_pipeline(device, swapchain, pipeline_layout, shader_stages, VK_POLYGON_MODE_LINE, true);

    VkPipeline bunny_pipeline_fill = create_pipeline(device, swapchain, pipeline_layout_bunny, shader_stages_bunny, VK_POLYGON_MODE_FILL, false);
    VkPipeline bunny_pipeline_grid = create_pipeline(device, swapchain, pipeline_layout_bunny, shader_stages_bunny, VK_POLYGON_MODE_LINE, false);

    auto prev_frame = imr_get_time_nano();
    float delta = 0;

    while (!glfwWindowShouldClose(window)) {
        swapchain.beginFrame([&](imr::Swapchain::Frame& frame) {
            camera_update(window, &camera_input);
            bool toggle_flight = false;
            camera_move_freelook(&camera, &camera_input, &camera_state, delta,
                    { &render_mode,
                      &fog_dropoff_lower,
                      &fog_dropoff_upper,
                      &fog_power,
                      &fog_lower_old,
                      &fog_upper_old,
                      &fog_power_old,
                      &tess_factor,
                      &update_tess,
                      &toggle_flight,
                    });
            if (toggle_flight) {
                flight = flight ^ 1;
                time_offset = get_timestep();
            }

            auto& image = frame.image();
            auto& depth_image = frame.depth_image();

            VkCommandBuffer cmdbuf;
            vkAllocateCommandBuffers(device.device, tmpPtr((VkCommandBufferAllocateInfo) {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .commandPool = device.pool,
                .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount = 1,
            }), &cmdbuf);

            vkBeginCommandBuffer(cmdbuf, tmpPtr((VkCommandBufferBeginInfo) {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            }));

            vk.cmdPipelineBarrier2KHR(cmdbuf, tmpPtr((VkDependencyInfo) {
                .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .dependencyFlags = 0,
                .imageMemoryBarrierCount = 1,
                .pImageMemoryBarriers = tmpPtr((VkImageMemoryBarrier2) {
                    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                    .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
                    .srcAccessMask = VK_ACCESS_2_NONE,
                    .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                    .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                    .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                    .newLayout = VK_IMAGE_LAYOUT_GENERAL,
                    .image = image.handle(),
                    .subresourceRange = image.whole_image_subresource_range(),
                }),
            }));
            vk.cmdPipelineBarrier2KHR(cmdbuf, tmpPtr((VkDependencyInfo) {
                .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .dependencyFlags = 0,
                .imageMemoryBarrierCount = 1,
                .pImageMemoryBarriers = tmpPtr((VkImageMemoryBarrier2) {
                    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                    .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
                    .srcAccessMask = VK_ACCESS_2_NONE,
                    .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                    .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                    .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                    .newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                    .image = depth_image.handle(),
                    .subresourceRange = depth_image.whole_image_subresource_range(),
                }),
            }));

            VkImageView imageView = create_image_view(device, image, VK_IMAGE_ASPECT_COLOR_BIT);
            VkImageView depthView = create_image_view(device, depth_image, VK_IMAGE_ASPECT_DEPTH_BIT);


            //Begin rendering pass


            VkRenderingAttachmentInfoKHR color_attachment_info = initializers::rendering_attachment_info();
            color_attachment_info.imageView = imageView;
            color_attachment_info.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR;
            color_attachment_info.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            color_attachment_info.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            color_attachment_info.clearValue = {.color = { 0.8f, 0.9f, 1.0f, 1.0f}};
            //color_attachment_info.clearValue = {.color = { 0.7f, 0.7f, 0.7f, 1.0f}};

            VkRenderingAttachmentInfoKHR depth_attachment_info = initializers::rendering_attachment_info();
            depth_attachment_info.imageView = depthView;
            depth_attachment_info.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR;
            depth_attachment_info.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            depth_attachment_info.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            depth_attachment_info.clearValue = {.depthStencil = { 1.0f, 0 }};

            VkRenderingInfoKHR render_info = initializers::rendering_info(
                initializers::rect2D(static_cast<int>(image.size().width), static_cast<int>(image.size().height), 0, 0),
                1,
                &color_attachment_info
            );
            render_info.pDepthAttachment = &depth_attachment_info;
            render_info.layerCount = 1;

            if (hasStencilComponent(depth_format)) {
                VkRenderingAttachmentInfoKHR stencil_attachment_info = initializers::rendering_attachment_info();
                stencil_attachment_info.imageView = depthView;
                stencil_attachment_info.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR;
                stencil_attachment_info.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                stencil_attachment_info.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

                render_info.pStencilAttachment = &stencil_attachment_info;
            }

            vkCmdBeginRenderingKHR(cmdbuf, &render_info);

            VkViewport viewport = initializers::viewport(static_cast<float>(image.size().width), static_cast<float>(image.size().height), 0.0f, 1.0f);
            vkCmdSetViewport(cmdbuf, 0, 1, &viewport);

            VkRect2D scissor = initializers::rect2D(static_cast<int>(image.size().width), static_cast<int>(image.size().height), 0, 0);
            vkCmdSetScissor(cmdbuf, 0, 1, &scissor);


            if (flight)
                set_camera_to_timestep(image);

            mat4 camera_matrix = camera_get_view_mat4(&camera, image.size().width, image.size().height);

            if (update_tess)
                camera_control_matrix = camera_matrix;

            //pass 1: Render the landscape
            switch (render_mode) {
            case FILL: vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline_fill); break;
            case GRID: vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline_grid); break;
            default: vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline_fill); break;
            }

            //vkCmdPushConstants(cmdbuf, pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, 4 * 16, &camera_matrix);
            //vkCmdPushConstants(cmdbuf, pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 4*16, 4 * 3, &camera.position);
            //vkCmdPushConstants(cmdbuf, pipeline_layout, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, 0, 4 * 16, &camera_matrix);
            vkCmdPushConstants(cmdbuf, pipeline_layout, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, 0, 4 * 16, &camera_matrix);
            vkCmdPushConstants(cmdbuf, pipeline_layout, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, 4 * 16, 4 * 16, &camera_control_matrix);
            vkCmdPushConstants(cmdbuf, pipeline_layout, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, 4 * 32, 4, &tess_factor);
            struct {
                int fog_power;
                float fog_dropoff_lower;
                float fog_dropoff_upper;
            } fog_constants { fog_power, fog_dropoff_lower, fog_dropoff_upper };
            vkCmdPushConstants(cmdbuf, pipeline_layout, VK_SHADER_STAGE_FRAGMENT_BIT, 4 * 33, 3 * 4, &fog_constants);

            VkBuffer vertexBuffers[] = {vertex_data_buffer->handle};
            VkDeviceSize offsets[] = {0};
            vkCmdBindVertexBuffers(cmdbuf, 0, 1, vertexBuffers, offsets);

            vkCmdDraw(cmdbuf, static_cast<uint32_t>(vertex_data_cpu.size()), 1, 0, 0);


            //pass 2: Render the bunny
            if (update_tess) {
                switch (render_mode) {
                case FILL: vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, bunny_pipeline_fill); break;
                case GRID: vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, bunny_pipeline_grid); break;
                default: vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline_fill); break;
                }

                vkCmdPushConstants(cmdbuf, pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, 4 * 16, &camera_matrix);
                vkCmdPushConstants(cmdbuf, pipeline_layout, VK_SHADER_STAGE_FRAGMENT_BIT, 4 * 16, 3 * 4, &fog_constants);

                VkBuffer vertexBuffers2[] = {bunny_model.triangles_gpu->handle};
                VkDeviceSize offsets2[] = {0};
                vkCmdBindVertexBuffers(cmdbuf, 0, 1, vertexBuffers2, offsets2);

                vkCmdDraw(cmdbuf, static_cast<uint32_t>(bunny_model.triangles.size()) * 3, 1, 0, 0);
            }

            vkCmdEndRenderingKHR(cmdbuf);


            //End rendering pass


            vk.cmdPipelineBarrier2KHR(cmdbuf, tmpPtr((VkDependencyInfo) {
                .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .dependencyFlags = 0,
                .imageMemoryBarrierCount = 1,
                .pImageMemoryBarriers = tmpPtr((VkImageMemoryBarrier2) {
                    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                    .srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT,
                    .srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT,
                    .dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                    .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
                    .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
                    .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                    .image = image.handle(),
                    .subresourceRange = image.whole_image_subresource_range(),
                }),
            }));

            VkFence fence;
            vkCreateFence(device.device, tmpPtr((VkFenceCreateInfo) {
                .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                .flags = 0,
            }), nullptr, &fence);

            vkEndCommandBuffer(cmdbuf);
            vkQueueSubmit(device.main_queue, 1, tmpPtr((VkSubmitInfo) {
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                .waitSemaphoreCount = 1,
                .pWaitSemaphores = &frame.swapchain_image_available,
                .pWaitDstStageMask = tmpPtr((VkPipelineStageFlags) VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT),
                .commandBufferCount = 1,
                .pCommandBuffers = &cmdbuf,
                .signalSemaphoreCount = 1,
                .pSignalSemaphores = &frame.signal_when_ready,
            }), fence);

            //printf("Frame submitted with fence = %llx\n", fence);

            frame.addCleanupFence(fence);
            frame.addCleanupAction([=, &device]() {
                //vkWaitForFences(context.device, 1, &fence, true, UINT64_MAX);
                vkDestroyFence(device.device, fence, nullptr);
                vkDestroyImageView(device.device, imageView, nullptr);
                vkDestroyImageView(device.device, depthView, nullptr);
                vkFreeCommandBuffers(device.device, device.pool, 1, &cmdbuf);
            });
            frame.queuePresent();

            auto now = imr_get_time_nano();
            delta = ((float) ((now - prev_frame) / 1000L)) / 1000000.0f;
            prev_frame = now;
        });

        fps_counter.tick();
        fps_counter.updateGlfwWindowTitle(window);
        glfwPollEvents();
    }

    vkDeviceWaitIdle(device.device);

    vkDestroyPipeline(device.device, graphics_pipeline_fill, nullptr);
    vkDestroyPipeline(device.device, graphics_pipeline_grid, nullptr);

    return 0;
}
