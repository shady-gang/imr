#include "swapchain_private.h"

namespace imr {

void Swapchain::Frame::presentFromBuffer(VkBuffer buffer, std::vector<VkSemaphore> waits) {
    auto& slot = _impl->slot;
    auto& swapchain = slot.impl_->swapchain;
    auto& device = _impl->device;
    auto& vk = device.dispatch;

    auto cmd = std::make_shared<imr::CommandBuffer>(device, device._impl->main_queue);

    vk.cmdPipelineBarrier2KHR(*cmd, tmpPtr<VkDependencyInfo>({
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .dependencyFlags = 0,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = tmpPtr<VkImageMemoryBarrier2>({
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
            .srcAccessMask = VK_ACCESS_2_NONE,
            .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .image = slot.image().handle(),
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .levelCount = 1,
                .layerCount = 1,
            }
        }),
    }));
    VkExtent2D src_size = swapchain._impl->swapchain.extent;
    vkCmdCopyBufferToImage(*cmd, buffer, slot.image().handle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, tmpPtr<VkBufferImageCopy>({
        .imageSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .layerCount = 1,
        },
        .imageExtent = {
            .width = swapchain._impl->swapchain.extent.width,
            .height = swapchain._impl->swapchain.extent.height,
            .depth = 1
        }
    }));
    vk.cmdPipelineBarrier2KHR(*cmd, tmpPtr<VkDependencyInfo>({
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .dependencyFlags = 0,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = tmpPtr<VkImageMemoryBarrier2>({
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            .dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            .image = slot.image().handle(),
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .levelCount = 1,
                .layerCount = 1,
            }
        }),
    }));

    VkSemaphore present_semaphore = _impl->create_frame_lived_semaphore("copy_buffer_to_swapchain_image");
    cmd->submit(waits, { present_semaphore });

    addCleanupAction([cmd]() {});

    slot.queuePresent({ present_semaphore });
}

void Swapchain::Frame::presentFromImage(VkImage image, std::vector<VkSemaphore> waits, VkImageLayout src_layout, std::optional<VkExtent2D> image_size) {
    auto& slot = _impl->slot;
    auto& swapchain = slot.impl_->swapchain;
    auto& device = _impl->device;
    auto& vk = device.dispatch;

    assert(image != slot.image().handle());

    auto cmd = std::make_shared<imr::CommandBuffer>(device, device._impl->main_queue);

    vk.cmdPipelineBarrier2KHR(*cmd, tmpPtr<VkDependencyInfo>({
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .dependencyFlags = 0,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = tmpPtr<VkImageMemoryBarrier2>({
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
            .srcAccessMask = VK_ACCESS_2_NONE,
            .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .image = slot.image().handle(),
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .levelCount = 1,
                .layerCount = 1,
            }
        }),
    }));
    VkExtent2D src_size;
    if (image_size)
        src_size = *image_size;
    else
        src_size = swapchain._impl->swapchain.extent;
    vkCmdBlitImage(*cmd, image, src_layout, slot.image().handle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, tmpPtr<VkImageBlit>({
        .srcSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .layerCount = 1,
        },
        .srcOffsets = {
            {},
            {
                .x = (int32_t) src_size.width,
                .y = (int32_t) src_size.height,
                .z = 1,
            }
        },
        .dstSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .layerCount = 1,
        },
        .dstOffsets = {
            {},
            {
                .x = (int32_t) swapchain._impl->swapchain.extent.width,
                .y = (int32_t) swapchain._impl->swapchain.extent.height,
                .z = 1,
            },
        }
    }), VK_FILTER_LINEAR);
    vk.cmdPipelineBarrier2KHR(*cmd, tmpPtr<VkDependencyInfo>({
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .dependencyFlags = 0,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = tmpPtr<VkImageMemoryBarrier2>({
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            .dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            .image = slot.image().handle(),
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .levelCount = 1,
                .layerCount = 1,
            }
        }),
    }));

    VkSemaphore present_semaphore = _impl->create_frame_lived_semaphore("copy_image_to_swapchain_image");
    cmd->submit(waits, { present_semaphore });

    addCleanupAction([cmd]() {});

    slot.queuePresent({ present_semaphore });
}


}