#include "imr/imr.h"
#include "imr/util.h"

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    auto window = glfwCreateWindow(1024, 1024, "Example", nullptr, nullptr);

    imr::Context context;
    imr::Device device(context);
    imr::Swapchain swapchain(device, window);
    imr::FpsCounter fps_counter;

    while (!glfwWindowShouldClose(window)) {
        // This helper function asks the swapchain for an image and prepares for rendering to it
        // It also allocates a commandbuffer for us, and prepares it for command recording
        //
        // Then it runs the user code (the lambda function).
        //
        // After that it submits the commands to the GPU, prepares the image to be shown and puts it in the queue to be displayed.
        swapchain.renderFrameSimplified([&](imr::Swapchain::SimplifiedRenderContext& context) {
            // Get the image and command buffer out of the context object
            auto& image = context.image();
            auto cmdbuf = context.cmdbuf();

            // Just clear the image to red
            VkClearColorValue red = { /* Red, Green, Blue, Alpha */ .float32 = { 1.0f, 0.0f, 0.0f, 1.0f}, };
            vkCmdClearColorImage(cmdbuf, image.handle(),
                 // For now all images are in the general layout as far as we're concerned
                 VK_IMAGE_LAYOUT_GENERAL,
                 // paint it red
                 &red,
                 // We want to clear the "whole" image, not a subresource within it.
                 1, tmpPtr(image.whole_image_subresource_range()));
        });

        // This tracks the fps (cpu-side)
        fps_counter.tick();
        fps_counter.updateGlfwWindowTitle(window);
        // We need to call this to know if someone tried to close the window
        glfwPollEvents();
    }

    return 0;
}
