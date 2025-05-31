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

struct push_constants {
    Tri tri = {
        { 0, 0 },
        { 1, 0 },
        { 1, 1 }
    };
    mat4 matrix;
    float time;
} push_constants;

Camera camera;
CameraFreelookState camera_state = {
    .fly_speed = 1.0f,
    .mouse_sensitivity = 1,
};
CameraInput camera_input;

void camera_update(GLFWwindow*, CameraInput* input);

bool reload_shaders = false;

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    auto window = glfwCreateWindow(1024, 1024, "Example", nullptr, nullptr);

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
        if (action == GLFW_PRESS && key == GLFW_KEY_R && (mods & GLFW_MOD_CONTROL)) {
            reload_shaders = true;
        }
    });

    imr::Context context;
    imr::Device device(context);
    imr::Swapchain swapchain(device, window);
    imr::FpsCounter fps_counter;
    std::unique_ptr<imr::ComputeShader> shader = std::make_unique<imr::ComputeShader>(device, "15_compute_cubes.spv");

    auto cube = make_cube();

    auto prev_frame = imr_get_time_nano();
    float delta = 0;

    camera = {{0, 0, 3}, {0, 0}, 60};

    std::vector<vec3> cube_positions;
    for (int i = 0; i < 1; i++) {
        vec3 position;
        position.x = -5 + 10 * (float) rand() / RAND_MAX;
        position.y = -5 + 10 * (float) rand() / RAND_MAX;
        position.z = -5 + 10 * (float) rand() / RAND_MAX;
        cube_positions.push_back(position);
    }

    auto& vk = device.dispatch;
    while (!glfwWindowShouldClose(window)) {
        fps_counter.tick();
        fps_counter.updateGlfwWindowTitle(window);

        swapchain.renderFrameSimplified([&](imr::Swapchain::SimplifiedRenderContext& context) {
            camera_update(window, &camera_input);
            camera_move_freelook(&camera, &camera_input, &camera_state, delta);

            auto& image = context.image();
            auto cmdbuf = context.cmdbuf();

            vk.cmdClearColorImage(cmdbuf, image.handle(), VK_IMAGE_LAYOUT_GENERAL, tmpPtr((VkClearColorValue) {
                .float32 = { 0.0f, 0.0f, 0.0f, 1.0f },
            }), 1, tmpPtr(image.whole_image_subresource_range()));

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

            if (reload_shaders) {
                swapchain.drain();
                auto replacement = std::make_unique<imr::ComputeShader>(device, "15_compute_cubes.spv");
                std::swap(shader, replacement);
                reload_shaders = false;
            }

            vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE, shader->pipeline());
            auto shader_bind_helper = shader->create_bind_helper();
            shader_bind_helper->set_storage_image(0, 0, image);
            shader_bind_helper->commit(cmdbuf);

            // update the push constant data on the host...
            mat4 m = identity_mat4;
            mat4 flip_y = identity_mat4;
            flip_y.rows[1][1] = -1;
            m = m * flip_y;
            mat4 view_mat = camera_get_view_mat4(&camera, context.image().size().width, context.image().size().height);
            m = m * view_mat;

            m = m * translate_mat4(vec3(-0.5, -0.5f, -0.5f));
            push_constants.time = ((imr_get_time_nano() / 1000) % 10000000000) / 1000000.0f;
            push_constants.matrix = m;

            //printf("\n\n");
            //printf("%f %f %f %f\n", m.elems.m00, m.elems.m01, m.elems.m02, m.elems.m03);
            //printf("%f %f %f %f\n", m.elems.m10, m.elems.m11, m.elems.m12, m.elems.m13);
            //printf("%f %f %f %f\n", m.elems.m20, m.elems.m21, m.elems.m22, m.elems.m23);
            //printf("%f %f %f %f\n", m.elems.m30, m.elems.m31, m.elems.m32, m.elems.m33);

            for (auto pos : cube_positions) {
                mat4 cm = translate_mat4(pos);
                cm = m * cm;
                push_constants.matrix = cm;
                for (int i = 0; i < 1; i++) {
                    auto tri = cube.triangles[i];
                    tri.color.x = i;
                    push_constants.tri = tri;
                    push_constants.time = ((imr_get_time_nano() / 1000) % 10000000000) / 1000000.0f;
                    // copy it to the command buffer!
                    vkCmdPushConstants(cmdbuf, shader->layout(), VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push_constants), &push_constants);

                    // dispatch like before
                    vkCmdDispatch(cmdbuf, (image.size().width + 31) / 32, (image.size().height + 31) / 32, 1);

                    vk.cmdPipelineBarrier2KHR(cmdbuf, tmpPtr((VkDependencyInfo) {
                        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                        .dependencyFlags = 0,
                        .imageMemoryBarrierCount = 1,
                        .pImageMemoryBarriers = tmpPtr((VkImageMemoryBarrier2) {
                            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                            .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                            .srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
                            .dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                            .dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
                            .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
                            .newLayout = VK_IMAGE_LAYOUT_GENERAL,
                            .image = image.handle(),
                            .subresourceRange = image.whole_image_subresource_range(),
                        })
                    }));
                }
            }

            context.addCleanupAction([=, &device]() {
                delete shader_bind_helper;
            });

            auto now = imr_get_time_nano();
            delta = ((float) ((now - prev_frame) / 1000L)) / 1000000.0f;
            prev_frame = now;

            glfwPollEvents();
        });
    }

    swapchain.drain();
    return 0;
}
