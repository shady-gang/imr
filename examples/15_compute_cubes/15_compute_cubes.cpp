#include "imr/imr.h"
#include "imr/util.h"

#include <cmath>
#include "nasl/nasl.h"
#include "nasl/nasl_mat.h"

#include "../common/camera.h"

using namespace nasl;

struct Tri { vec3 v0, v1, v2; vec3 color; };

struct Cube {
    Tri triangles[12];
};

Cube make_cube() {
    /*
     *  +Y
     *  ^
     *  |
     *  |
     *  D------C.
     *  |\     |\
     *  | H----+-G
     *  | |    | |
     *  A-+----B | ---> +X
     *   \|     \|
     *    E------F
     *     \
     *      \
     *       \
     *        v +Z
     *
     * Adapted from
     * https://www.asciiart.eu/art-and-design/geometries
     */
    vec3 A = { 0, 0, 0 };
    vec3 B = { 1, 0, 0 };
    vec3 C = { 1, 1, 0 };
    vec3 D = { 0, 1, 0 };
    vec3 E = { 0, 0, 1 };
    vec3 F = { 1, 0, 1 };
    vec3 G = { 1, 1, 1 };
    vec3 H = { 0, 1, 1 };

    int i = 0;
    Cube cube = {};

    auto add_face = [&](vec3 v0, vec3 v1, vec3 v2, vec3 v3, vec3 color) {
        /*
         * v0 --- v3
         *  |   / |
         *  |  /  |
         *  | /   |
         * v1 --- v2
         */
        cube.triangles[i++] = { v0, v1, v3, color };
        cube.triangles[i++] = { v1, v2, v3, color };
    };

    // top face
    add_face(H, D, C, G, vec3(0, 1, 0));
    // north face
    add_face(A, B, C, D, vec3(1, 0, 0));
    // west face
    add_face(A, D, H, E, vec3(0, 0, 1));
    // east face
    add_face(F, G, C, B, vec3(1, 0, 1));
    // south face
    add_face(E, H, G, F, vec3(0, 1, 1));
    // bottom face
    add_face(E, F, B, A, vec3(1, 1, 0));
    assert(i == 12);
    return cube;
}

struct {
    Tri tri = {
        { 0, 0 },
        { 1, 0 },
        { 1, 1 }
    };
    mat4 matrix;
    float time;
} push_constants_single;

struct {
    VkDeviceAddress tri_buffer;
    uint32_t tri_count;
    mat4 matrix;
    float time;
} push_constants_batched;

struct {
    VkDeviceAddress tri_buffer;
    uint32_t tri_count;
    VkDeviceAddress matrices_buffer;
    uint32_t instances_count;
    float time;
} push_constants_instanced;

struct {
    VkDeviceAddress tri_buffer;
    uint32_t tri_count;
    VkDeviceAddress matrices_buffer;
    uint32_t instances_count;
    VkDeviceAddress preprocessed_tri_buffer;
    float time;
} push_constants_pipelined_vert;

struct {
    VkDeviceAddress preprocessed_tri_buffer;
    uint32_t tri_count;
} push_constants_pipelined_frag;

Camera camera;
CameraFreelookState camera_state = {
    .fly_speed = 1.0f,
    .mouse_sensitivity = 1,
};
CameraInput camera_input;

void camera_update(GLFWwindow*, CameraInput* input);

bool reload_shaders = false;

#define INSTANCES_COUNT 16

enum TriDrawMode {
    SINGLE,
    BATCHED,
    INSTANCED,
    PIPELINED,
};

struct PreprocessedTri {
    vec4 v0;
    vec4 v1;
    vec4 v2;
    vec2 ss_v0;
    vec2 ss_v1;
    vec2 ss_v2;
    vec3 color;
};

TriDrawMode mode = SINGLE;

struct Shaders {
    imr::ComputePipeline single;
    imr::ComputePipeline batched;
    imr::ComputePipeline instanced;
    imr::ComputePipeline pipelined_triangles;
    imr::ComputePipeline pipelined_raster;

    Shaders(imr::Device& d) :
        single(d, "15_compute_cubes.spv"),
        batched(d, "15_compute_cubes_batched.spv"),
        instanced(d, "15_compute_cubes_instanced.spv"),
        pipelined_triangles(d, "15_compute_cubes_pipelined_triangles.spv"),
        pipelined_raster(d, "15_compute_cubes_pipelined_raster.spv")
        {}
};

int main(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--batched") == 0) {
            mode = BATCHED;
        }
        if (strcmp(argv[i], "--instanced") == 0) {
            mode = INSTANCED;
        }
        if (strcmp(argv[i], "--pipelined") == 0) {
            mode = PIPELINED;
        }
    }

    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    auto window = glfwCreateWindow(1024, 1024, "Example", nullptr, nullptr);

    glfwSetKeyCallback(window, [](GLFWwindow* window, int key, int scancode, int action, int mods) {
        if (key == GLFW_KEY_R && (mods & GLFW_MOD_CONTROL))
            reload_shaders = true;
    });

    imr::Context context;
    imr::Device device(context);
    imr::Swapchain swapchain(device, window);
    imr::FpsCounter fps_counter;
    auto shaders = std::make_unique<Shaders>(device);

    auto cube = make_cube();

    std::unique_ptr<imr::Buffer> triangles_buffer;
    if (mode == BATCHED || mode == INSTANCED || mode == PIPELINED) {
        triangles_buffer = std::make_unique<imr::Buffer>(device, sizeof(cube.triangles), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
        triangles_buffer->uploadDataSync(0, sizeof(cube.triangles), cube.triangles);
    }

    std::unique_ptr<imr::Buffer> matrices_buffer;
    if (mode == INSTANCED || mode == PIPELINED) {
        matrices_buffer = std::make_unique<imr::Buffer>(device, sizeof(nasl::mat4) * INSTANCES_COUNT, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
    }

    std::unique_ptr<imr::Buffer> tmp_buffer;
    if (mode == PIPELINED) {
        // we're never writing to this from the host
        tmp_buffer = std::make_unique<imr::Buffer>(device, sizeof(PreprocessedTri) * INSTANCES_COUNT * 12, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
    }

    std::vector<vec3> positions;

    for (size_t i = 0; i < INSTANCES_COUNT; i++) {
        vec3 p;
        p.x = ((float)rand() / RAND_MAX) * 20 - 10;
        p.y = ((float)rand() / RAND_MAX) * 20 - 10;
        p.z = ((float)rand() / RAND_MAX) * 20 - 10;
        positions.push_back(p);
    }

    auto prev_frame = imr_get_time_nano();
    float delta = 0;

    camera = {{0, 0, 3}, {0, 0}, 60};

    std::unique_ptr<imr::Image> depthBuffer;

    auto& vk = device.dispatch;
    while (!glfwWindowShouldClose(window)) {
        fps_counter.tick();
        fps_counter.updateGlfwWindowTitle(window);

        swapchain.renderFrameSimplified([&](imr::Swapchain::SimplifiedRenderContext& context) {
            camera_update(window, &camera_input);
            camera_move_freelook(&camera, &camera_input, &camera_state, delta);

            if (reload_shaders) {
                swapchain.drain();
                shaders = std::make_unique<Shaders>(device);
                reload_shaders = false;
            }

            auto& image = context.image();
            auto& cmdbuf = context.cmdbuf();

            if (!depthBuffer || depthBuffer->size().width != context.image().size().width || depthBuffer->size().height != context.image().size().height) {
                VkImageUsageFlagBits depthBufferFlags = static_cast<VkImageUsageFlagBits>(VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT);
                depthBuffer = std::make_unique<imr::Image>(device, VK_IMAGE_TYPE_2D, context.image().size(), VK_FORMAT_R32_SFLOAT, depthBufferFlags);

                vk.cmdPipelineBarrier2KHR(cmdbuf, tmpPtr((VkDependencyInfo) {
                    .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                    .dependencyFlags = 0,
                    .imageMemoryBarrierCount = 1,
                    .pImageMemoryBarriers = tmpPtr((VkImageMemoryBarrier2) {
                        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                        .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                        .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                        .dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                        .dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
                        .image = depthBuffer->handle(),
                        .subresourceRange = depthBuffer->whole_image_subresource_range()
                    })
                }));
            }

            vk.cmdClearColorImage(cmdbuf, image.handle(), VK_IMAGE_LAYOUT_GENERAL, tmpPtr((VkClearColorValue) {
                .float32 = { 0.0f, 0.0f, 0.0f, 1.0f },
            }), 1, tmpPtr(image.whole_image_subresource_range()));

            vk.cmdClearColorImage(cmdbuf, depthBuffer->handle(), VK_IMAGE_LAYOUT_GENERAL, tmpPtr((VkClearColorValue) {
                .float32 = { 1.0f, 0.0f, 0.0f, 0.0f },
            }), 1, tmpPtr(depthBuffer->whole_image_subresource_range()));

            // This barrier ensures that the clear is finished before we run the dispatch.
            // before: all writes from the "transfer" stage (to which the clear command belongs)
            // after: all writes from the "compute" stage
            vk.cmdPipelineBarrier2KHR(cmdbuf, tmpPtr((VkDependencyInfo) {
                .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .dependencyFlags = 0,
                .memoryBarrierCount = 1,
                .pMemoryBarriers = tmpPtr((VkMemoryBarrier2) {
                    .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
                    .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                    .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                    .dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                    .dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                })
            }));

            auto add_render_barrier = [&]() {
                vk.cmdPipelineBarrier2KHR(cmdbuf, tmpPtr((VkDependencyInfo) {
                   .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                   .dependencyFlags = 0,
                   .memoryBarrierCount = 1,
                   .pMemoryBarriers = tmpPtr((VkMemoryBarrier2) {
                       .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
                       .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                       .srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
                       .dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                       .dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
                   })
               }));
            };

            // update the push constant data on the host...
            mat4 m = identity_mat4;
            mat4 flip_y = identity_mat4;
            flip_y.rows[1][1] = -1;
            m = m * flip_y;
            mat4 view_mat = camera_get_view_mat4(&camera, context.image().size().width, context.image().size().height);
            m = m * view_mat;
            m = m * translate_mat4(vec3(-0.5, -0.5f, -0.5f));

            switch (mode) {
                case SINGLE: {
                    auto& shader = shaders->single;
                    vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE, shader.pipeline());
                    auto shader_bind_helper = shader.create_bind_helper();
                    shader_bind_helper->set_storage_image(0, 0, image.whole_image_view());
                    shader_bind_helper->set_storage_image(0, 1, depthBuffer->whole_image_view());
                    shader_bind_helper->commit(cmdbuf);

                    push_constants_single.time = ((imr_get_time_nano() / 1000) % 10000000000) / 1000000.0f;

                    for (auto pos : positions) {
                        mat4 cube_matrix = m;
                        cube_matrix = cube_matrix * translate_mat4(pos);

                        for (int i = 0; i < 12; i++) {
                            add_render_barrier();

                            auto tri = cube.triangles[i];

                            push_constants_single.tri = tri;
                            push_constants_single.matrix = cube_matrix;
                            // copy it to the command buffer!
                            vkCmdPushConstants(cmdbuf, shader.layout(), VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push_constants_single), &push_constants_single);

                            // dispatch like before
                            vkCmdDispatch(cmdbuf, (image.size().width + 31) / 32, (image.size().height + 31) / 32, 1);
                        }
                    }

                    cmdbuf.addCleanupAction([=, &device]() {
                        delete shader_bind_helper;
                    });
                    break;
                }
                case BATCHED: {
                    auto& shader = shaders->batched;
                    vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE, shader.pipeline());
                    auto shader_bind_helper = shader.create_bind_helper();
                    shader_bind_helper->set_storage_image(0, 0, image.whole_image_view());
                    shader_bind_helper->set_storage_image(0, 1, depthBuffer->whole_image_view());
                    shader_bind_helper->commit(cmdbuf);

                    push_constants_batched.time = ((imr_get_time_nano() / 1000) % 10000000000) / 1000000.0f;
                    // the cube data is the same for all
                    push_constants_batched.tri_buffer = triangles_buffer->device_address();
                    push_constants_batched.tri_count = 12;

                    for (auto pos : positions) {
                        add_render_barrier();

                        mat4 cube_matrix = m;
                        cube_matrix = cube_matrix * translate_mat4(pos);

                        push_constants_batched.matrix = cube_matrix;

                        vkCmdPushConstants(cmdbuf, shader.layout(), VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push_constants_batched), &push_constants_batched);
                        vkCmdDispatch(cmdbuf, (image.size().width + 31) / 32, (image.size().height + 31) / 32, 1);
                    }

                    break;
                }
                case INSTANCED: {
                    auto& shader = shaders->instanced;
                    vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE, shader.pipeline());
                    auto shader_bind_helper = shader.create_bind_helper();
                    shader_bind_helper->set_storage_image(0, 0, image.whole_image_view());
                    shader_bind_helper->set_storage_image(0, 1, depthBuffer->whole_image_view());
                    shader_bind_helper->commit(cmdbuf);

                    push_constants_instanced.time = ((imr_get_time_nano() / 1000) % 10000000000) / 1000000.0f;
                    // the cube data is the same for all
                    push_constants_instanced.tri_buffer = triangles_buffer->device_address();
                    push_constants_instanced.tri_count = 12;

                    std::vector<mat4> matrices;
                    for (auto pos : positions) {
                        mat4 cube_matrix = m;
                        cube_matrix = cube_matrix * translate_mat4(pos);
                        matrices.push_back(cube_matrix);
                    }
                    matrices_buffer->uploadDataSync(0, sizeof(mat4) * matrices.size(), matrices.data());

                    push_constants_instanced.matrices_buffer = matrices_buffer->device_address();
                    push_constants_instanced.instances_count = matrices.size();

                    add_render_barrier();

                    vkCmdPushConstants(cmdbuf, shader.layout(), VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push_constants_instanced), &push_constants_instanced);
                    vkCmdDispatch(cmdbuf, (image.size().width + 31) / 32, (image.size().height + 31) / 32, 1);
                    break;
                }
                case PIPELINED: {
                    auto& triangle_transform_shader = shaders->pipelined_triangles;
                    vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE, triangle_transform_shader.pipeline());

                    push_constants_pipelined_vert.time = ((imr_get_time_nano() / 1000) % 10000000000) / 1000000.0f;
                    // the cube data is the same for all
                    push_constants_pipelined_vert.tri_buffer = triangles_buffer->device_address();
                    push_constants_pipelined_vert.tri_count = 12;

                    std::vector<mat4> matrices;
                    for (auto pos : positions) {
                        mat4 cube_matrix = m;
                        cube_matrix = cube_matrix * translate_mat4(pos);
                        matrices.push_back(cube_matrix);
                    }
                    matrices_buffer->uploadDataSync(0, sizeof(mat4) * matrices.size(), matrices.data());

                    push_constants_pipelined_vert.matrices_buffer = matrices_buffer->device_address();
                    push_constants_pipelined_vert.instances_count = matrices.size();
                    push_constants_pipelined_vert.preprocessed_tri_buffer = tmp_buffer->device_address();

                    add_render_barrier();

                    vkCmdPushConstants(cmdbuf, triangle_transform_shader.layout(), VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push_constants_pipelined_vert), &push_constants_pipelined_vert);
                    vkCmdDispatch(cmdbuf, (12 + 31) / 32, (INSTANCES_COUNT + 31) / 32, 1);

                    add_render_barrier();

                    auto& rasterizer_shader = shaders->pipelined_raster;
                    vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE, rasterizer_shader.pipeline());
                    auto shader_bind_helper = rasterizer_shader.create_bind_helper();
                    shader_bind_helper->set_storage_image(0, 0, image.whole_image_view());
                    shader_bind_helper->set_storage_image(0, 1, depthBuffer->whole_image_view());
                    shader_bind_helper->commit(cmdbuf);

                    push_constants_pipelined_frag.preprocessed_tri_buffer = tmp_buffer->device_address();
                    push_constants_pipelined_frag.tri_count = INSTANCES_COUNT * 12;

                    vkCmdPushConstants(cmdbuf, rasterizer_shader.layout(), VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push_constants_pipelined_frag), &push_constants_pipelined_frag);

                    vkCmdDispatch(cmdbuf, (image.size().width + 31) / 32, (image.size().height + 31) / 32, 1);
                    break;
                }
            }

            auto now = imr_get_time_nano();
            delta = ((float) ((now - prev_frame) / 1000L)) / 1000000.0f;
            prev_frame = now;

            glfwPollEvents();
        });
    }

    swapchain.drain();
    return 0;
}
