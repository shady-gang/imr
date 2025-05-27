#include "imr/imr.h"
#include "imr/util.h"

#include <cmath>
#include <nasl.h>
#include <nasl_mat.h>

using namespace nasl;

#include "VkBootstrap.h"

#include <ctime>
#include <memory>
#include <filesystem>

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
    float time;
} push_constants;

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    auto window = glfwCreateWindow(1024, 1024, "Example", nullptr, nullptr);

    imr::Context context;
    imr::Device device(context);
    imr::Swapchain swapchain(device, window);
    imr::FpsCounter fps_counter;
    imr::ComputeShader shader(device, "14_compute_cube.spv");

    auto cube = make_cube();

    auto& vk = device.dispatch;
    while (!glfwWindowShouldClose(window)) {
        fps_counter.tick();
        fps_counter.updateGlfwWindowTitle(window);

        swapchain.renderFrameSimplified([&](imr::Swapchain::SimplifiedRenderContext& context) {
            auto& image = context.image();
            auto cmdbuf = context.cmdbuf();

            vk.cmdClearColorImage(cmdbuf, image.handle(), VK_IMAGE_LAYOUT_GENERAL, tmpPtr((VkClearColorValue) {
                .float32 = { 0.0f, 0.0f, 0.0f, 1.0f },
            }), 1, tmpPtr(image.whole_image_subresource_range()));

            // This barrier doesn't change the layout of the image, and instead just ensures that the clear is finished before we run the dispatch.
            // before: all writes from the "transfer" stage (to which the clear command belongs)
            // after: all writes from the "compute" stage
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
                    .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
                    .newLayout = VK_IMAGE_LAYOUT_GENERAL,
                    .image = image.handle(),
                    .subresourceRange = image.whole_image_subresource_range()
                }),
            }));

            vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE, shader.pipeline());
            auto shader_bind_helper = shader.create_bind_helper();
            shader_bind_helper->set_storage_image(0, 0, image);
            shader_bind_helper->commit(cmdbuf);

            for (int i = 0; i < 12; i++) {
                auto tri = cube.triangles[i];
                // update the push constant data on the host...
                mat4 m = identity_mat4;
                mat4 flip_y = identity_mat4;
                flip_y.rows[1][1] = -1;
                m = m * flip_y;
                m = m * rotate_axis_mat4(0, 0.2f);
                m = m * rotate_axis_mat4(1, push_constants.time);
                m = m * translate_mat4(vec3(-0.5, -0.5f, -0.5f));
                Tri transformed;
                auto transform = [&](vec3 input) -> vec3 {
                    vec4 v = vec4(input, 1);
                    v = m * v;
                    v.xyz = vec3(v.xyz) / (float) v.w;
                    return v.xyz;
                };
                transformed.v0 = transform(tri.v0);
                transformed.v1 = transform(tri.v1);
                transformed.v2 = transform(tri.v2);
                transformed.color = tri.color;

                push_constants.tri = transformed;
                push_constants.time = ((imr_get_time_nano() / 1000) % 10000000000) / 1000000.0f;
                // copy it to the command buffer!
                vkCmdPushConstants(cmdbuf, shader.layout(), VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push_constants), &push_constants);

                // dispatch like before
                vkCmdDispatch(cmdbuf, (image.size().width + 31) / 32, (image.size().height + 31) / 32, 1);
            }

            context.addCleanupAction([=, &device]() {
                delete shader_bind_helper;
            });
        });

        glfwPollEvents();
    }

    swapchain.drain();
    return 0;
}
