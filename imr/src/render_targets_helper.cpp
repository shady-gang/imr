#include "swapchain_private.h"

namespace imr {

void Swapchain::Frame::withRenderTargets(VkCommandBuffer cmdbuf, std::vector<Image*> color_images, Image* depth, std::function<void()> f) {
    auto& device = _impl->device;

    std::vector<VkImageView> color_views;
    color_views.resize(color_images.size());
    size_t i = 0;

    std::optional<std::tuple<size_t, size_t>> size;
    auto set_size = [&](VkExtent3D extents) {
        if (!size)
            size = std::make_tuple(extents.width, extents.height);
        else {
            assert(extents.width == std::get<0>(*size));
            assert(extents.height == std::get<1>(*size));
        }
    };

    for (auto color_image : color_images) {
        vkCreateImageView(device.device, tmpPtr<VkImageViewCreateInfo>({
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = color_image->handle(),
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = color_image->format(),
            .subresourceRange = color_image->whole_image_subresource_range(),
        }), nullptr, &color_views.data()[i]);

        set_size(color_image->size());

        addCleanupAction([=,&device]() {
            vkDestroyImageView(device.device, color_views.data()[i], nullptr);
        });
        i++;
    }

    VkImageView depth_view = VK_NULL_HANDLE;
    if (depth) {
        vkCreateImageView(device.device, tmpPtr<VkImageViewCreateInfo>({
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = depth->handle(),
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = depth->format(),
            .subresourceRange = depth->whole_image_subresource_range(),
        }), nullptr, &depth_view);

        set_size(depth->size());

        addCleanupAction([=, &device]() {
            vkDestroyImageView(device.device, depth_view, nullptr);
        });
    }

    assert(size);
    uint32_t width = std::get<0>(*size);
    uint32_t height = std::get<1>(*size);

    std::vector<VkRenderingAttachmentInfo> color_attachments;
    for (auto color_view : color_views) {
        color_attachments.push_back({
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView = color_view,
            .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
            .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        });
    }

    VkRenderingAttachmentInfo depth_attachment = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = depth_view,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
    };

    vkCmdBeginRendering(cmdbuf, tmpPtr<VkRenderingInfo>({
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .flags = 0,
        .renderArea = {
            .extent = {
                .width = width,
                .height = height,
            },
        },
        .layerCount = 1,
        .viewMask = 0,
        .colorAttachmentCount = static_cast<uint32_t>(color_attachments.size()),
        .pColorAttachments = color_attachments.data(),
        .pDepthAttachment = depth ? &depth_attachment : nullptr,
    }));

    VkViewport viewport {
        .width = static_cast<float>(width),
        .height = static_cast<float>(height),
        .maxDepth = 1.0f,
    };
    vkCmdSetViewport(cmdbuf, 0, 1, &viewport);
    VkRect2D scissor = {
        .extent = {
            .width = width,
            .height = height,
        }
    };
    vkCmdSetScissor(cmdbuf, 0, 1, &scissor);

    f();

    vkCmdEndRendering(cmdbuf);
}

}