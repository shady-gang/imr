#include "imr_private.h"
#include "imr/util.h"

#include "VkBootstrap.h"

#include <functional>
#include <vector>
#include <algorithm>
#include <thread>
#include <chrono>

namespace imr {

#define CHECK_VK_THROW(do) CHECK_VK(do, throw std::exception())

struct SwapchainSlot;

struct Swapchain::Impl {
    Swapchain& parent;
    Device& device;
    GLFWwindow* window = nullptr;
    Impl(Swapchain& parent, Device&, GLFWwindow*);
    ~Impl();

    VkSurfaceKHR surface;
    size_t frame_counter = 0;

    uint64_t last_present = 0;
    bool should_resize = false;

    vkb::Swapchain swapchain;
    std::vector<std::unique_ptr<SwapchainSlot>> slots;

    imr::Image* depth_image;

    void build_swapchain();
    void destroy_swapchain();
};

struct SwapchainSlot {
    Swapchain& swapchain;
    SwapchainSlot(Swapchain& s) : swapchain(s) {
        auto& device = s._impl->device;
        auto& vk = device.dispatch;

        CHECK_VK_THROW(vkCreateSemaphore(device.device, tmp((VkSemaphoreCreateInfo) {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        }), nullptr, &copy_done));

        vk.setDebugUtilsObjectNameEXT(tmp((VkDebugUtilsObjectNameInfoEXT) {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
            .objectType = VK_OBJECT_TYPE_SEMAPHORE,
            .objectHandle = reinterpret_cast<uint64_t>(copy_done),
            .pObjectName = "SwapchainSlot::copy_done"
        }));
    }
    SwapchainSlot(SwapchainSlot&) = delete;

    VkImage image;
    uint32_t image_index;

    VkSemaphore copy_done;
    VkFence wait_for_previous_present = VK_NULL_HANDLE;

    std::unique_ptr<Swapchain::Frame> frame = nullptr;

    ~SwapchainSlot();
};

SwapchainSlot::~SwapchainSlot() {
    auto& device = swapchain._impl->device;
    if (wait_for_previous_present) {
        CHECK_VK_THROW(vkWaitForFences(device.device, 1, &wait_for_previous_present, true, UINT64_MAX));
        vkDestroyFence(device.device, wait_for_previous_present, nullptr);
        wait_for_previous_present = nullptr;
    }
    vkDestroySemaphore(device.device, copy_done, nullptr);
    if (wait_for_previous_present)
        vkDestroyFence(device.device, wait_for_previous_present, nullptr);
}

struct Swapchain::Frame::Impl {
    Device& device;
    SwapchainSlot& slot;
    imr::Image& depth_image;

    std::vector<VkFence> cleanup_fences;
    std::vector<std::function<void(void)>> cleanup_queue;
};

Swapchain::Swapchain(Device& device, GLFWwindow* window) {
    auto& vk = device.dispatch;

    _impl = std::make_unique<Swapchain::Impl>(*this, device, window);
    _impl->build_swapchain();
}

VkFormat Swapchain::format() const {
    return _impl->swapchain.image_format;
}

Swapchain::Impl::Impl(Swapchain& parent, Device& device, GLFWwindow* window) : parent(parent), device(device), window(window) {
    CHECK_VK_THROW(glfwCreateWindowSurface(device.context.instance, window, nullptr, &surface));
}

static VkFormat findSupportedFormat(imr::Device& device, const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
    for (VkFormat format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(device.physical_device, format, &props);

        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
            return format;
        } else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
            return format;
        }
    }

    throw std::runtime_error("failed to find supported format!");
}

void Swapchain::Impl::build_swapchain() {
    uint32_t surface_formats_count;
    CHECK_VK_THROW(vkGetPhysicalDeviceSurfaceFormatsKHR(device.physical_device, surface, &surface_formats_count, nullptr));

    std::vector<VkSurfaceFormatKHR> formats;
    formats.resize(surface_formats_count);
    CHECK_VK_THROW(vkGetPhysicalDeviceSurfaceFormatsKHR(device.physical_device, surface, &surface_formats_count, formats.data()));

    std::optional<VkSurfaceFormatKHR> preferred;
    for (auto format : formats) {
        if (format.format == VK_FORMAT_R8G8B8A8_UNORM)
            preferred = format;
        if (format.format == VK_FORMAT_B8G8R8A8_UNORM)
            preferred = format;
    }

    if (!preferred) {
        fprintf(stderr, "Swapchain format is not 8-bit RGBA or BGRA");
    }

    auto depth_format = findSupportedFormat(device,
            {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    depth_image = new imr::Image(device, VK_IMAGE_TYPE_2D, {width, height, 1}, depth_format, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);

    auto builder = vkb::SwapchainBuilder(device.physical_device, device.device, surface);
    builder.add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT);
    builder.set_desired_extent(width, height);
    //builder.set_desired_present_mode(VK_PRESENT_MODE_IMMEDIATE_KHR);
    builder.set_desired_present_mode(VK_PRESENT_MODE_MAILBOX_KHR);
    if (preferred)
        builder.set_desired_format(*preferred);

    if (auto built = builder.build(); built.has_value()) {
        swapchain = built.value();
        fprintf(stdout, "Allocated swapchain with %d images\n", swapchain.image_count);
    } else {
        fprintf(stderr, "Failed to build a swapchain (size=%d,%d, error=%d).\n", width, height, built.vk_result());
        throw std::exception();
    }

    for (int i = 0; i < swapchain.image_count; i++) {
        slots.emplace_back(std::make_unique<SwapchainSlot>(parent));
    }
}

void Swapchain::Impl::destroy_swapchain() {
    slots.clear();
    delete depth_image;
    vkb::destroy_swapchain(swapchain);
}

Swapchain::Impl::~Impl() {
    vkDestroySurfaceKHR(device.context.dispatch.instance, surface, nullptr);
}

/// Acquires the next image
static std::optional<std::tuple<SwapchainSlot&, VkSemaphore>> nextSwapchainSlot(Swapchain::Impl* _impl) {
    auto& device = _impl->device;
    auto& vk = device.dispatch;

    uint32_t image_index;

    VkSemaphore image_acquired_semaphore;
    CHECK_VK_THROW(vkCreateSemaphore(device.device, tmp((VkSemaphoreCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    }), nullptr, &image_acquired_semaphore));

    vk.setDebugUtilsObjectNameEXT(tmp((VkDebugUtilsObjectNameInfoEXT) {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .objectType = VK_OBJECT_TYPE_SEMAPHORE,
        .objectHandle = reinterpret_cast<uint64_t>(image_acquired_semaphore),
        .pObjectName = "SwapchainSlot::image_acquired"
    }));

    VkFence fence;
    CHECK_VK_THROW(vkCreateFence(device.device, tmp((VkFenceCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
    }), nullptr, &fence));

    VkResult acquire_result = device.dispatch.acquireNextImageKHR(_impl->swapchain, UINT64_MAX, image_acquired_semaphore, fence, &image_index);
    switch (acquire_result) {
        case VK_SUCCESS: break;
        case VK_SUBOPTIMAL_KHR: _impl->should_resize = true; break;
        case VK_ERROR_OUT_OF_DATE_KHR: {
            fprintf(stderr, "Acquire failed. We need to resize!\n");
            vkDestroySemaphore(device.device, image_acquired_semaphore, nullptr);
            vkDestroyFence(device.device, fence, nullptr);
            return std::nullopt;
        }
        default:
            fprintf(stderr, "Acquire result was: %d\n", acquire_result);
            throw std::exception();
    }

    // We know the next image !
    SwapchainSlot& slot = *_impl->slots[image_index];
    //printf("Image acquired: %d\n", image_index);
    slot.image = _impl->swapchain.get_images().value()[image_index];
    slot.image_index = image_index;

    VkFence prev_fence = slot.wait_for_previous_present;
    slot.wait_for_previous_present = fence;

    // let's recycle the resources that last slot used...
    // First make sure the _previous_ present is finished.
    // We could also set and wait on an acquire fence, but the validation layers are apparently not convinced this is sufficiently safe...
    if (prev_fence) {
        CHECK_VK_THROW(vkWaitForFences(device.device, 1, &prev_fence, true, UINT64_MAX));
        vkDestroyFence(device.device, prev_fence, nullptr);
    }
    //printf("Waited for %llx\n", (uint64_t) slot.wait_for_previous_present);

    slot.frame.reset();

    return std::tie<SwapchainSlot&, VkSemaphore>(slot, image_acquired_semaphore);
}

void Swapchain::Frame::add_to_delete_queue(std::optional<VkFence> fence, std::function<void()>&& fn) {
    if (fence)
        _impl->cleanup_fences.push_back(*fence);
    _impl->cleanup_queue.push_back(std::move(fn));
}

Swapchain::Frame::Frame(Impl&& impl) {
    _impl = std::make_unique<Frame::Impl>(impl);
    swapchain_image = _impl->slot.image;
    depth_image = _impl->depth_image.handle;
}

void Swapchain::resize() {
    _impl->should_resize = true;
}

void Swapchain::beginFrame(std::function<void(Swapchain::Frame&)>&& fn) {
    auto& device = _impl->device;
    while (true) {
        if (_impl->should_resize) {
            _impl->should_resize = false;
            glfwPollEvents();
            drain();
            _impl->destroy_swapchain();
            _impl->build_swapchain();
        }
        auto result = nextSwapchainSlot(&*_impl);
        if (!result) {
            _impl->should_resize = true;
            continue;
        }
        auto [slot, acquired] = *result;
        slot.frame.reset();
        slot.frame = std::make_unique<Frame>(std::move(Frame::Impl(device, slot, *_impl->depth_image)));
        slot.frame->swapchain_image_available = acquired;
        slot.frame->id = _impl->frame_counter++;
        slot.frame->width = _impl->swapchain.extent.width;
        slot.frame->height = _impl->swapchain.extent.height;
        assert(acquired);
        slot.frame->_impl->cleanup_queue.emplace_back([=, &device]() {
            vkDestroySemaphore(device.device, acquired, nullptr);
        });

        //printf("Preparing frame: %d\n", slot.frame->id);
        fn(*slot.frame);
        break;
    }
}

Swapchain::Frame::~Frame() {
    //printf("Recycling frame %d in slot %d\n", id, _impl->slot.image_index);
    // Before we can cleanup the resources we need to wait on the relevant fences
    // for now let's just wait on ALL of them at once
    if (!_impl->cleanup_fences.empty()) {
        for (auto fence : _impl->cleanup_fences) {
            //printf("Waited on fence = %llx\n", fence);
            CHECK_VK_THROW(vkWaitForFences(_impl->device.device, 1, &fence, true, UINT64_MAX));
        }
        _impl->cleanup_fences.clear();
    }

    // We want to iterate over the queue in a FIFO manner
    std::reverse(_impl->cleanup_queue.begin(), _impl->cleanup_queue.end());
    for (auto& fn : _impl->cleanup_queue) {
        fn();
    }
    _impl->cleanup_queue.clear();
}

void Swapchain::Frame::presentFromBuffer(VkBuffer buffer, VkFence signal_when_reusable, std::optional<VkSemaphore> sem) {
    auto& slot = _impl->slot;
    auto& swapchain = slot.swapchain;
    auto& device = _impl->device;
    auto& vk = device.dispatch;

    assert(signal_when_reusable != VK_NULL_HANDLE);

    std::vector<VkSemaphore> semaphores;
    semaphores.push_back(swapchain_image_available);
    if (sem)
        semaphores.push_back(*sem);

    VkCommandBuffer cmdbuf;
    CHECK_VK_THROW(vkAllocateCommandBuffers(device.device, tmp((VkCommandBufferAllocateInfo) {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = device.pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    }), &cmdbuf));

    CHECK_VK_THROW(vkBeginCommandBuffer(cmdbuf, tmp((VkCommandBufferBeginInfo) {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    })));

    vk.cmdPipelineBarrier2KHR(cmdbuf, tmp((VkDependencyInfo) {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .dependencyFlags = 0,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = tmp((VkImageMemoryBarrier2) {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
            .srcAccessMask = VK_ACCESS_2_NONE,
            .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .image = slot.image,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .levelCount = 1,
                .layerCount = 1,
            }
        }),
    }));
    VkExtent2D src_size = swapchain._impl->swapchain.extent;
    vkCmdCopyBufferToImage(cmdbuf, buffer, slot.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, tmp((VkBufferImageCopy) {
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
    vk.cmdPipelineBarrier2KHR(cmdbuf, tmp((VkDependencyInfo) {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .dependencyFlags = 0,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = tmp((VkImageMemoryBarrier2) {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            .dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            .image = slot.image,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .levelCount = 1,
                .layerCount = 1,
            }
        }),
    }));

    std::vector<VkPipelineStageFlags> stage_flags;
    for (auto& sem : semaphores)
        stage_flags.emplace_back(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);

    vkEndCommandBuffer(cmdbuf);
    vkQueueSubmit(device.main_queue, 1, tmp((VkSubmitInfo) {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = static_cast<uint32_t>(semaphores.size()),
        .pWaitSemaphores = semaphores.data(),
        .pWaitDstStageMask = stage_flags.data(),
        .commandBufferCount = 1,
        .pCommandBuffers = &cmdbuf,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &slot.copy_done,
    }), signal_when_reusable);

    add_to_delete_queue(std::nullopt, [=, &device]() {
        vkFreeCommandBuffers(device.device, device.pool, 1, &cmdbuf);
    });

    present(slot.copy_done);
}

void Swapchain::Frame::presentFromImage(VkImage image, VkFence signal_when_reusable, std::optional<VkSemaphore> sem, VkImageLayout src_layout, std::optional<VkExtent2D> image_size) {
    auto& slot = _impl->slot;
    auto& swapchain = slot.swapchain;
    auto& device = _impl->device;
    auto& vk = device.dispatch;

    std::vector<VkSemaphore> semaphores;
    semaphores.push_back(swapchain_image_available);
    if (sem)
        semaphores.push_back(*sem);

    assert(image != slot.image);
    assert(signal_when_reusable != VK_NULL_HANDLE);

    VkCommandBuffer cmdbuf;
    vkAllocateCommandBuffers(device.device, tmp((VkCommandBufferAllocateInfo) {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = device.pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    }), &cmdbuf);

    vkBeginCommandBuffer(cmdbuf, tmp((VkCommandBufferBeginInfo) {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    }));

    vk.cmdPipelineBarrier2KHR(cmdbuf, tmp((VkDependencyInfo) {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .dependencyFlags = 0,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = tmp((VkImageMemoryBarrier2) {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
            .srcAccessMask = VK_ACCESS_2_NONE,
            .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .image = slot.image,
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
    vkCmdBlitImage(cmdbuf, image, src_layout, slot.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, tmp((VkImageBlit) {
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
    vk.cmdPipelineBarrier2KHR(cmdbuf, tmp((VkDependencyInfo) {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .dependencyFlags = 0,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = tmp((VkImageMemoryBarrier2) {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            .dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            .image = slot.image,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .levelCount = 1,
                .layerCount = 1,
            }
        }),
    }));

    std::vector<VkPipelineStageFlags> stage_flags;
    for (auto& sem : semaphores)
        stage_flags.emplace_back(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);

    vkEndCommandBuffer(cmdbuf);
    vkQueueSubmit(device.main_queue, 1, tmp((VkSubmitInfo) {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = static_cast<uint32_t>(semaphores.size()),
        .pWaitSemaphores = semaphores.data(),
        .pWaitDstStageMask = stage_flags.data(),
        .commandBufferCount = 1,
        .pCommandBuffers = &cmdbuf,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &slot.copy_done,
    }), signal_when_reusable);

    // abort();

    semaphores.clear();
    semaphores.emplace_back(slot.copy_done);

    add_to_delete_queue(std::nullopt, [=, &device]() {
        vkFreeCommandBuffers(device.device, device.pool, 1, &cmdbuf);
    });

    present(slot.copy_done);
}

void Swapchain::Frame::present(std::optional<VkSemaphore> sem) {
    auto& slot = _impl->slot;
    auto& swapchain = slot.swapchain;
    auto& device = _impl->device;

    uint64_t now = imr_get_time_nano();
    uint64_t delta = now - swapchain._impl->last_present;
    int64_t delta_us = (int64_t)(delta / 1000);

    int64_t min_delta = int64_t(1000000.0 / swapchain.maxFps);
    //printf("delta: %zu us, min_delta = %zu \n", delta_us, min_delta);
    int64_t sleep_time = min_delta - delta_us;
    if (sleep_time > 0) {
        //printf("we're too fast. throttling by: %zu us\n", sleep_time);
        std::this_thread::sleep_for(std::chrono::microseconds(sleep_time));
    }

    swapchain._impl->last_present = now;

    //printf("Presenting in slot: %d\n", slot.image_index);

    std::vector<VkSemaphore> semaphores;
    if (sem)
        semaphores.push_back(*sem);

    VkResult present_result = vkQueuePresentKHR(device.main_queue, tmp((VkPresentInfoKHR) {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = static_cast<uint32_t>(semaphores.size()),
        .pWaitSemaphores = semaphores.data(),
        .swapchainCount = 1,
        .pSwapchains = &swapchain._impl->swapchain.swapchain,
        .pImageIndices = tmp(slot.image_index),
    }));
    //printf("Queued presentation, will signal %llx\n", (uint64_t) slot.wait_for_previous_present);
    switch (present_result) {
        case VK_SUCCESS:
        case VK_SUBOPTIMAL_KHR: break;
        case VK_ERROR_OUT_OF_DATE_KHR: {
            fprintf(stderr, "Present failed. We need to resize!\n");
            break;
        }
        default: throw std::exception();
    }
}

void Swapchain::drain() {
    auto& device = _impl->device;
    vkDeviceWaitIdle(device.device);

    for (auto& slot : _impl->slots)
        slot->frame.reset();
    //_impl->prev_frames.clear();
}

Swapchain::~Swapchain() {
    drain();

    _impl->destroy_swapchain();
    _impl.reset();
}

}
