#include "imr/imr.h"
#include "imr/util.h"

#include <fstream>
#include <filesystem>

#include "VkBootstrap.h"

#include "libs/initializers.h"
#include "libs/tooling.h"

#include "libs/camera.h"
#include "libs/model.h"
#include "libs/scene.h"

#ifdef SPACENAVD
#include <spnav.h>
#endif

struct CommandArguments {
    bool use_glsl = true;
    std::optional<float> camera_speed;
    std::optional<vec3> camera_eye;
    std::optional<vec2> camera_rotation;
    std::optional<float> camera_fov;
};

static Scene* global_scene;
void glfw_key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS && key == GLFW_KEY_F4) {
        printf("--position %f %f %f --rotation %f %f --fov %f\n", (float) global_scene->camera.position.x, (float) global_scene->camera.position.y, (float) global_scene->camera.position.z, (float) global_scene->camera.rotation.yaw, (float) global_scene->camera.rotation.pitch, (float) global_scene->camera.fov);
    }
    if ((action == GLFW_PRESS || action == GLFW_REPEAT) && key == GLFW_KEY_MINUS) {
        //global_scene->camera.fov -= 2.0f;
        global_scene->tess_factor -= int(global_scene->tess_factor / 10) + 1;
        printf("Tesselation now %f\n", global_scene->tess_factor);
    }
    if ((action == GLFW_PRESS || action == GLFW_REPEAT) && key == GLFW_KEY_EQUAL) {
        //global_scene->camera.fov += 2.0f;
        global_scene->tess_factor += int(global_scene->tess_factor / 10) + 1;
        printf("Tesselation now %f\n", global_scene->tess_factor);
    }
    if ((action == GLFW_PRESS || action == GLFW_REPEAT) && key == GLFW_KEY_1) {
        global_scene->fog_power -= 1;
        printf("Fog power now %d\n", global_scene->fog_power);
    }
    if ((action == GLFW_PRESS || action == GLFW_REPEAT) && key == GLFW_KEY_2) {
        global_scene->fog_power += 1;
        printf("Fog power now %d\n", global_scene->fog_power);
    }
    if ((action == GLFW_PRESS || action == GLFW_REPEAT) && key == GLFW_KEY_3) {
        global_scene->fog_dropoff_lower -= (1 - global_scene->fog_dropoff_lower) * 0.1f;
        printf("Fog lower now %f\n", global_scene->fog_dropoff_lower);
    }
    if ((action == GLFW_PRESS || action == GLFW_REPEAT) && key == GLFW_KEY_4) {
        global_scene->fog_dropoff_lower += (1 - global_scene->fog_dropoff_lower) * 0.1f;
        printf("Fog lower now %f\n", global_scene->fog_dropoff_lower);
    }
    if ((action == GLFW_PRESS || action == GLFW_REPEAT) && key == GLFW_KEY_5) {
        global_scene->fog_dropoff_upper -= (1 - global_scene->fog_dropoff_upper) * 0.1f;
        printf("Fog upper now %f\n", global_scene->fog_dropoff_upper);
    }
    if ((action == GLFW_PRESS || action == GLFW_REPEAT) && key == GLFW_KEY_6) {
        global_scene->fog_dropoff_upper += (1 - global_scene->fog_dropoff_upper) * 0.1f;
        printf("Fog upper now %f\n", global_scene->fog_dropoff_upper);
    }

    if (action == GLFW_PRESS && key == GLFW_KEY_R) {
        global_scene->fog_dropoff_lower = 0.98;
        global_scene->fog_dropoff_upper = 0.995;
        global_scene->fog_power = 10;
        global_scene->tess_factor = 25.0f;
        printf("Reset fog and tessellation\n");
    }

    if (action == GLFW_PRESS && key == GLFW_KEY_F) {
        if (global_scene->fog_dropoff_lower == 1.0f) {
            global_scene->fog_dropoff_lower = global_scene->fog_lower_old;
            global_scene->fog_dropoff_upper = global_scene->fog_upper_old;
            global_scene->fog_power = global_scene->fog_power_old;
        } else {
            global_scene->fog_lower_old = global_scene->fog_dropoff_lower;
            global_scene->fog_upper_old = global_scene->fog_dropoff_upper;
            global_scene->fog_power_old = global_scene->fog_power;

            global_scene->fog_dropoff_lower = 1.0f;
            global_scene->fog_dropoff_upper = 1.0f;
            global_scene->fog_power = 1;
        }
    }

    if (action == GLFW_PRESS && key == GLFW_KEY_M) {
        switch (global_scene->render_mode) {
        case FILL: global_scene->render_mode = GRID; break;
        case GRID: global_scene->render_mode = FILL; break;
        default: global_scene->render_mode = FILL; break;
        }
    }

    if (action == GLFW_PRESS && key == GLFW_KEY_SPACE) {
        global_scene->flight = global_scene->flight ^ 1;

        global_scene->time_offset = global_scene->get_timestep();
    }

    if (action == GLFW_PRESS && key == GLFW_KEY_0) {
        global_scene->update_tess = global_scene->update_tess ^ 1;
        if (global_scene->update_tess)
            printf("Tesselation cammera updating\n");
        else
            printf("Tesselation cammera stopped\n");
    }
}

int main(int argc, char ** argv) {
    char * scene_filename = nullptr;
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
        scene_filename = argv[i];
    }

    if (!scene_filename) {
        printf("Usage: ./ra <scene>\n");
        exit(-1);
    }

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

    struct Scene scene(scene_filename, device, swapchain, cmd_args.use_glsl);

    auto depth_format = swapchain.depth_format();


    //scene.camera = model.loaded_camera;
    scene.camera.position = cmd_args.camera_eye.value_or(scene.camera.position);
    if (cmd_args.camera_rotation.has_value()) {
        scene.camera.rotation.yaw = cmd_args.camera_rotation.value().x;
        scene.camera.rotation.pitch = cmd_args.camera_rotation.value().y;
    }
    scene.camera.fov = cmd_args.camera_fov.value_or(scene.camera.fov);
    scene.camera_state.fly_speed = cmd_args.camera_speed.value_or(scene.camera_state.fly_speed);

    global_scene = &scene;
    glfwSetKeyCallback(window, glfw_key_callback);

    auto vkCmdBeginRenderingKHR = reinterpret_cast<PFN_vkCmdBeginRenderingKHR>(vkGetInstanceProcAddr(context.instance, "vkCmdBeginRenderingKHR"));
    auto vkCmdEndRenderingKHR   = reinterpret_cast<PFN_vkCmdEndRenderingKHR>(vkGetInstanceProcAddr(context.instance, "vkCmdEndRenderingKHR"));

    auto& vk = device.dispatch;

    auto prev_frame = imr_get_time_nano();
    float delta = 0;

    while (!glfwWindowShouldClose(window)) {
        swapchain.beginFrame([&](imr::Swapchain::Frame& frame) {
            CameraInput camera_input;
            camera_update(window, &camera_input);
            camera_move_freelook(&scene, &camera_input, delta);

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


            scene.render_to_cmdbuf(cmdbuf, image, frame);


            //End rendering pass
            vkCmdEndRenderingKHR(cmdbuf);

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

#if SPACENAVD
    spnav_close();
#endif

    return 0;
}
