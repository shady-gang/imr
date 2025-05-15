#include "imr/imr.h"

namespace imr {

struct SimplifiedRenderContextImpl : Swapchain::SimplifiedRenderContext {
    Swapchain::Frame& frame;
    VkCommandBuffer command_buffer;

    SimplifiedRenderContextImpl(Swapchain::Frame& frame, VkCommandBuffer cmdbuf) : frame(frame), command_buffer(cmdbuf) {};

    Image& image() const override;
    VkCommandBuffer cmdbuf() const override;

    void addCleanupAction(std::function<void(void)>&& fn) override;
};

Image& SimplifiedRenderContextImpl::image() const { return frame.image(); }
VkCommandBuffer SimplifiedRenderContextImpl::cmdbuf() const { return command_buffer; }

void SimplifiedRenderContextImpl::addCleanupAction(std::function<void()>&& fn) {
    frame.addCleanupAction(std::move(fn));
}

void Swapchain::renderFrameSimplified(std::function<void(SimplifiedRenderContext&)>&& fn) {
    auto& device = this->device();
    auto& vk = device.dispatch;

    beginFrame([&](Frame& frame) {
        auto& image = frame.image();

        // Allocate and begin recording a command buffer
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

        // This barrier transitions the image from an unknown state into the "general" layout so we can render to it.
        // before the barrier: nothing relevant happens
        // after the barrier: all writes from any pipeline stage
        vk.cmdPipelineBarrier2KHR(cmdbuf, tmpPtr((VkDependencyInfo) {
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .dependencyFlags = 0,
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = tmpPtr((VkImageMemoryBarrier2) {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
                .srcAccessMask = VK_ACCESS_2_NONE,
                .dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .newLayout = VK_IMAGE_LAYOUT_GENERAL,
                .image = image.handle(),
                .subresourceRange = image.whole_image_subresource_range(),
            }),
        }));

        // Run user code
        SimplifiedRenderContextImpl context(frame, cmdbuf);
        fn(context);

        // This barrier transitions the image from the "general" layout into the "present src" layout so it can be shown
        // before the barrier: all writes from any pipeline stage
        // after the barrier: all reads from the present stage
        vk.cmdPipelineBarrier2KHR(cmdbuf, tmpPtr((VkDependencyInfo) {
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .dependencyFlags = 0,
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = tmpPtr((VkImageMemoryBarrier2) {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                .srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                .srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
                .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
                .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                .image = image.handle(),
                .subresourceRange = image.whole_image_subresource_range()
            }),
        }));

        // Create a fence so we can track the execution of the cmdbuf
        VkFence fence;
        vkCreateFence(device.device, tmpPtr((VkFenceCreateInfo) {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .flags = 0,
        }), nullptr, &fence);

        // Finish the cmdbuf and submit it to the GPU, and pass the fence so we're notified when it's done
        // before: wait on the swapchain image to be available
        // after: notify the swapchain that the image can be shown
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

        // cleanup those objects once the cmdbuf has executed
        frame.addCleanupFence(fence);
        frame.addCleanupAction([=, &device]() {
            vkDestroyFence(device.device, fence, nullptr);
            vkFreeCommandBuffers(device.device, device.pool, 1, &cmdbuf);
        });

        frame.queuePresent();
    });
}

}