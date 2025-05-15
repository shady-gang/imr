#include "imr/imr.h"
#include "imr/util.h"

#include "VkBootstrap.h"

#include <ctime>
#include <memory>
#include <filesystem>

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    auto window = glfwCreateWindow(1024, 1024, "Example", nullptr, nullptr);

    imr::Context context;
    imr::Device device(context);
    imr::Swapchain swapchain(device, window);
    imr::FpsCounter fps_counter;
    // this class takes care of various boilerplate setup for you
    imr::ComputeShader shader(device, "12_compute_shader.spv");

    auto& vk = device.dispatch;
    while (!glfwWindowShouldClose(window)) {
        fps_counter.tick();
        fps_counter.updateGlfwWindowTitle(window);

        swapchain.renderFrameSimplified([&](imr::Swapchain::SimplifiedRenderContext& context) {
            auto& image = context.image();
            auto cmdbuf = context.cmdbuf();

            vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE, shader.pipeline());
            // this helper class takes care of "descriptors"
            // it has to live as long as the frame rendering takes so it _cannot_ be stack-allocated here
            // instead it goes on the heap and we manually delete it
            auto shader_bind_helper = shader.create_bind_helper();
            shader_bind_helper->set_storage_image(0, 0, image);
            shader_bind_helper->commit(cmdbuf);

            // We dispatch invocations in "workgroups", whose size is defined in the compute shader file
            // we need to dispatch (screenSize / workgroupSize) workgroups, but rounding up if the screen size is not a multiple of the workgroup size
            // all sizes here are 3D but we use only the first two to match the screen size and make the "depth" dimension just one
            vkCmdDispatch(cmdbuf, (image.size().width + 31) / 32, (image.size().height + 31) / 32, 1);

            context.addCleanupAction([=, &device]() {
                delete shader_bind_helper;
            });
        });

        glfwPollEvents();
    }

    swapchain.drain();
    return 0;
}
