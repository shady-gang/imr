#include "imr_private.h"
#include "swapchain_private.h"

namespace imr {

struct SimplifiedRenderContextImpl : Swapchain::SimplifiedRenderContext {
    Swapchain::Frame& frame_;
    CommandBuffer& command_buffer;

    SimplifiedRenderContextImpl(Swapchain::Frame& frame, CommandBuffer& cmdbuf) : frame_(frame), command_buffer(cmdbuf) {};

    Image& image() const override;
    CommandBuffer& cmdbuf() const override;
    Swapchain::Frame& frame() const override;
};

Image& SimplifiedRenderContextImpl::image() const { return frame_.slot().image(); }
CommandBuffer& SimplifiedRenderContextImpl::cmdbuf() const { return command_buffer; }
Swapchain::Frame& SimplifiedRenderContextImpl::frame() const { return frame_; }

void Swapchain::renderFrameSimplified(std::function<void(SimplifiedRenderContext&)>&& fn) {
    auto& device = this->device();
    auto& vk = device.dispatch;

    beginFrame([&](Frame& frame) {
        auto& image = frame.slot().image();

        VkSemaphore frame_ready_to_present = frame.slot().impl_->present_semaphore;
        //VkSemaphore frame_ready_to_present = frame._impl->create_frame_lived_semaphore();

        auto cmd = std::make_shared<imr::CommandBuffer>(device, device._impl->main_queue);
        vk.cmdPipelineBarrier2KHR(*cmd, tmpPtr<VkDependencyInfo>({
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .dependencyFlags = 0,
            .memoryBarrierCount = 1,
            .pMemoryBarriers = tmpPtr<VkMemoryBarrier2>({
                .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
                .srcStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
                .srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                .dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT,
            }),
        }));

        // This barrier transitions the image from an unknown state into the "general" layout so we can render to it.
        // before the barrier: nothing relevant happens
        // after the barrier: all writes from any pipeline stage
        vk.cmdPipelineBarrier2KHR(*cmd, tmpPtr<VkDependencyInfo>({
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .dependencyFlags = 0,
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = tmpPtr<VkImageMemoryBarrier2>({
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                .srcStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
                .srcAccessMask = VK_ACCESS_2_NONE,
                .dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                .dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
                .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .newLayout = VK_IMAGE_LAYOUT_GENERAL,
                .image = image.handle(),
                .subresourceRange = image.whole_image_subresource_range(),
            }),
        }));

        // Run user code
        SimplifiedRenderContextImpl context(frame, *cmd);
        fn(context);

        // This barrier transitions the image from the "general" layout into the "present src" layout so it can be shown
        // before the barrier: all writes from any pipeline stage
        // after the barrier: all reads from the present stage
        vk.cmdPipelineBarrier2KHR(*cmd, tmpPtr<VkDependencyInfo>({
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .dependencyFlags = 0,
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = tmpPtr<VkImageMemoryBarrier2>({
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

        // Finish the cmdbuf and submit it to the GPU
        // before: wait on the swapchain image to be available
        // after: notify the swapchain that the image can be shown
        std::vector submit_signals = frame.signals;
        submit_signals.push_back(frame_ready_to_present);
        cmd->submit(frame.waits, submit_signals);

        // cleanup those objects once the cmdbuf has executed
        // frame.addCleanupFence(cmd->fence_);
        frame.addCleanupAction([cmd]() {});

        std::vector present_waits = frame.slot().waits;
        present_waits.push_back(frame_ready_to_present);
        frame.slot().queuePresent(present_waits);
    });
}

}