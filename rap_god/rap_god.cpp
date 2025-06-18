#include "imr/imr.h"
#include "imr/util.h"

#include <cmath>
#include "nasl/nasl.h"
#include "nasl/nasl_mat.h"

#include "../examples/common/camera.h"

using namespace nasl;

struct Vertex {
    vec3 pos;
    vec3 color;
    vec2 uv;
};
struct Tri { Vertex v0, v1, v2; };

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
        cube.triangles[i++] = { { v0, color, vec2(0, 0) }, { v1, color, vec2(0, 1) }, { v3, color, vec2(1, 0) } };
        cube.triangles[i++] = { { v1, color, vec2(0, 1) }, { v2, color, vec2(1, 1) }, { v3, color, vec2(1, 0) } };
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
    add_face(F, E, H, G, vec3(0, 1, 1));
    // bottom face
    add_face(A, E, F, B, vec3(1, 1, 0));
    assert(i == 12);
    return cube;
}

struct {
    mat4 matrix;
    float time;
} push_constants_batched;

Camera camera;
CameraFreelookState camera_state = {
    .fly_speed = 1.0f,
    .mouse_sensitivity = 1,
};
CameraInput camera_input;

void camera_update(GLFWwindow*, CameraInput* input);

bool reload_shaders = false;

#define INSTANCES_COUNT 1

struct Shaders {
    std::vector<std::string> files = { "rap_god.vert.spv", "rap_god.frag.spv" };

    std::vector<std::unique_ptr<imr::ShaderModule>> modules;
    std::vector<std::unique_ptr<imr::ShaderEntryPoint>> entry_points;
    std::unique_ptr<imr::GraphicsPipeline> pipeline;

    Shaders(imr::Device& d, imr::Swapchain& swapchain) {
        imr::GraphicsPipeline::RenderTargetsState rts;
        rts.color.push_back((imr::GraphicsPipeline::RenderTarget) {
            .format = swapchain.format(),
            .blending = {
                .blendEnable = true,
                .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
                .dstColorBlendFactor = VK_BLEND_FACTOR_ONE,
                .colorBlendOp = VK_BLEND_OP_ADD,
                .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
                .alphaBlendOp = VK_BLEND_OP_ADD,
                .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
            }
        });
        imr::GraphicsPipeline::RenderTarget depth = {
            .format = VK_FORMAT_D32_SFLOAT
        };
        rts.depth = depth;

        VkVertexInputBindingDescription bindings[] = {
            {
                .binding = 0,
                .stride = sizeof(Vertex),
                .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
            },
        };

        VkVertexInputAttributeDescription attributes[] = {
            {
                .location = 0,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = 0,
            },
            {
                .location = 1,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = offsetof(Vertex, color),
            },
            {
                .location = 2,
                .binding = 0,
                .format = VK_FORMAT_R32G32_SFLOAT,
                .offset = offsetof(Vertex, uv),
            },
        };

        VkPipelineVertexInputStateCreateInfo vertex_input {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .vertexBindingDescriptionCount = sizeof(bindings) / sizeof(bindings[0]),
            .pVertexBindingDescriptions = &bindings[0],
            .vertexAttributeDescriptionCount = sizeof(attributes) / sizeof(attributes[0]),
            .pVertexAttributeDescriptions = &attributes[0],
        };

        VkPipelineRasterizationStateCreateInfo rasterization {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,

            .polygonMode = VK_POLYGON_MODE_FILL,
            .cullMode = VK_CULL_MODE_NONE,
            .frontFace = VK_FRONT_FACE_CLOCKWISE,

            .lineWidth = 1.0f,
        };

        VkPipelineDepthStencilStateCreateInfo no_depth_test {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,

            .depthTestEnable = VK_TRUE,
            .depthWriteEnable = VK_FALSE,
            .depthCompareOp = VK_COMPARE_OP_LESS
        };

        imr::GraphicsPipeline::StateBuilder stateBuilder = {
            .vertexInputState = vertex_input,
            .inputAssemblyState = imr::GraphicsPipeline::simple_triangle_input_assembly(),
            .viewportState = imr::GraphicsPipeline::one_dynamically_sized_viewport(),
            .rasterizationState = rasterization,
            .multisampleState = imr::GraphicsPipeline::one_spp(),
            .depthStencilState = no_depth_test,
        };

        std::vector<imr::ShaderEntryPoint*> entry_point_ptrs;
        for (auto filename : files) {
            VkShaderStageFlagBits stage;
            if (filename.ends_with("vert.spv"))
                stage = VK_SHADER_STAGE_VERTEX_BIT;
            else if (filename.ends_with("frag.spv"))
                stage = VK_SHADER_STAGE_FRAGMENT_BIT;
            else
                throw std::runtime_error("Unknown suffix");
            modules.push_back(std::make_unique<imr::ShaderModule>(d, std::move(filename)));
            entry_points.push_back(std::make_unique<imr::ShaderEntryPoint>(*modules.back(), stage, "main"));
            entry_point_ptrs.push_back(entry_points.back().get());
        }
        pipeline = std::make_unique<imr::GraphicsPipeline>(d, std::move(entry_point_ptrs), rts, stateBuilder);
    }
};

int main(int argc, char** argv) {
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

    auto cube = make_cube();

    std::unique_ptr<imr::Buffer> vertex_buffer = std::make_unique<imr::Buffer>(device, sizeof(cube.triangles), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
    vertex_buffer->uploadDataSync(0, vertex_buffer->size, &cube.triangles);

    auto prev_frame = imr_get_time_nano();
    float delta = 0;

    camera = {{0, 0, 0}, {0, 0}, 60};

    std::unique_ptr<imr::Image> depthBuffer;

    auto shaders = std::make_unique<Shaders>(device, swapchain);

    auto& vk = device.dispatch;
    while (!glfwWindowShouldClose(window)) {
        fps_counter.tick();
        fps_counter.updateGlfwWindowTitle(window);

        swapchain.renderFrameSimplified([&](imr::Swapchain::SimplifiedRenderContext& context) {
            camera_update(window, &camera_input);
            camera_move_freelook(&camera, &camera_input, &camera_state, delta);

            if (reload_shaders) {
                swapchain.drain();
                shaders = std::make_unique<Shaders>(device, swapchain);
                reload_shaders = false;
            }

            auto& image = context.image();
            auto cmdbuf = context.cmdbuf();

            if (!depthBuffer || depthBuffer->size().width != context.image().size().width || depthBuffer->size().height != context.image().size().height) {
                VkImageUsageFlagBits depthBufferFlags = static_cast<VkImageUsageFlagBits>(VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
                depthBuffer = std::make_unique<imr::Image>(device, VK_IMAGE_TYPE_2D, context.image().size(), VK_FORMAT_D32_SFLOAT, depthBufferFlags);

                vk.cmdPipelineBarrier2KHR(cmdbuf, tmpPtr((VkDependencyInfo) {
                    .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                    .dependencyFlags = 0,
                    .imageMemoryBarrierCount = 1,
                    .pImageMemoryBarriers = tmpPtr((VkImageMemoryBarrier2) {
                        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                        .srcStageMask = 0,
                        .srcAccessMask = 0,
                        .dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                        .dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT,
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

            vk.cmdClearDepthStencilImage(cmdbuf, depthBuffer->handle(), VK_IMAGE_LAYOUT_GENERAL, tmpPtr((VkClearDepthStencilValue) {
                .depth = 1.0f,
                .stencil = 0,
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
                    .dstStageMask = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
                    .dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT,
                })
            }));

            // update the push constant data on the host...
            mat4 m = identity_mat4;
            mat4 flip_y = identity_mat4;
            flip_y.rows[1][1] = -1;
            m = m * flip_y;
            camera.fov = 90;
            mat4 view_mat = camera_get_view_mat4(&camera, context.image().size().width, context.image().size().height);
            m = m * view_mat;
            m = m * rotate_axis_mat4(1, push_constants_batched.time * 0.01f);
            m = m * rotate_axis_mat4(0, push_constants_batched.time * 0.021f + 0.33f);
            m = m * translate_mat4(vec3(-0.5, -0.5f, -0.5f));

            auto& pipeline = shaders->pipeline;
            vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline());
            vkCmdBindVertexBuffers(cmdbuf, 0, 1, &vertex_buffer->handle, tmpPtr((VkDeviceSize) 0));

            push_constants_batched.time = ((imr_get_time_nano() / 1000) % 10000000000) / 1000000.0f;

            context.frame().withRenderTargets(cmdbuf, { &image }, &*depthBuffer, [&]() {
                push_constants_batched.matrix = m;
                vkCmdPushConstants(cmdbuf, pipeline->layout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(push_constants_batched), &push_constants_batched);
                vkCmdDraw(cmdbuf, 12 * 3, INSTANCES_COUNT, 0, 0);
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
