#include "swapchain_private.h"
#include "imr/util.h"

#include "VkBootstrap.h"

#include <functional>
#include <vector>
#include <algorithm>
#include <thread>
#include <chrono>

namespace imr {

SwapchainSlot::SwapchainSlot(Swapchain& s) : swapchain(s) {
    auto& device = s._impl->device;
    auto& vk = device.dispatch;

    CHECK_VK_THROW(vkCreateSemaphore(device.device, tmpPtr((VkSemaphoreCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    }), nullptr, &copy_done));

    vk.setDebugUtilsObjectNameEXT(tmpPtr((VkDebugUtilsObjectNameInfoEXT) {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .objectType = VK_OBJECT_TYPE_SEMAPHORE,
        .objectHandle = reinterpret_cast<uint64_t>(copy_done),
        .pObjectName = "SwapchainSlot::copy_done"
    }));

    CHECK_VK_THROW(vkCreateSemaphore(device.device, tmpPtr((VkSemaphoreCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    }), nullptr, &present_semaphore));

    vk.setDebugUtilsObjectNameEXT(tmpPtr((VkDebugUtilsObjectNameInfoEXT) {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .objectType = VK_OBJECT_TYPE_SEMAPHORE,
        .objectHandle = reinterpret_cast<uint64_t>(copy_done),
        .pObjectName = "SwapchainSlot::present_queued"
    }));
}

SwapchainSlot::~SwapchainSlot() {
    auto& device = swapchain._impl->device;
    if (wait_for_previous_present) {
        CHECK_VK_THROW(vkWaitForFences(device.device, 1, &wait_for_previous_present, true, UINT64_MAX));
        vkDestroyFence(device.device, wait_for_previous_present, nullptr);
        wait_for_previous_present = nullptr;
    }
    vkDestroySemaphore(device.device, copy_done, nullptr);
    vkDestroySemaphore(device.device, present_semaphore, nullptr);
    if (wait_for_previous_present)
        vkDestroyFence(device.device, wait_for_previous_present, nullptr);
}

Swapchain::Swapchain(Device& device, GLFWwindow* window) {
    auto& vk = device.dispatch;

    _impl = std::make_unique<Swapchain::Impl>(*this, device, window);
    _impl->build_swapchain();
}

Swapchain::Impl::Impl(Swapchain& parent, Device& device, GLFWwindow* window) : parent(parent), device(device), window(window) {
    CHECK_VK_THROW(glfwCreateWindowSurface(device.context.instance, window, nullptr, &surface));
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

    std::optional<VkFormat> depth_format;
    for (VkFormat format : {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT}) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(device.physical_device, format, &props);

        if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
            depth_format = format;
    }

    if (!depth_format) {
        throw std::runtime_error("failed to find supported depth format!");
    }

    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

    VkExtent3D depth_image_size = {width, height, 1};
    depth_image = std::make_unique<Image>(device, VK_IMAGE_TYPE_2D, depth_image_size, *depth_format, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);

    auto builder = vkb::SwapchainBuilder(device.physical_device, device.device, surface);
    builder.add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT);
    builder.set_desired_extent(width, height);
    builder.set_desired_present_mode(VK_PRESENT_MODE_IMMEDIATE_KHR);
    //builder.set_desired_present_mode(VK_PRESENT_MODE_MAILBOX_KHR);
    //builder.set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR);
    if (preferred)
        builder.set_desired_format(*preferred);

    if (auto built = builder.build(); built.has_value()) {
        swapchain = built.value();
    } else {
        fprintf(stderr, "Failed to build a swapchain (size=%d,%d, error=%d).\n", width, height, built.vk_result());
        throw std::runtime_error("failure to build a swapchain");
    }

    for (int i = 0; i < swapchain.image_count; i++) {
        slots.emplace_back(std::make_unique<SwapchainSlot>(parent));
    }
}

void Swapchain::Impl::destroy_swapchain() {
    slots.clear();
    vkb::destroy_swapchain(swapchain);
}

Swapchain::Impl::~Impl() {
    vkDestroySurfaceKHR(device.context.dispatch.instance, surface, nullptr);
}

Device& Swapchain::device() const { return _impl->device; }

VkFormat Swapchain::format() const {
    return _impl->swapchain.image_format;
}

VkFormat Swapchain::depth_format() const {
    return _impl->depth_image->format();
}

Image& Swapchain::depth_image() const {
    return *_impl->depth_image;
}

/// Acquires the next image
std::optional<std::tuple<SwapchainSlot&, VkSemaphore>> nextSwapchainSlot(Swapchain::Impl* _impl) {
    auto& device = _impl->device;
    auto& vk = device.dispatch;

    uint32_t image_index;

    VkSemaphore image_acquired_semaphore;
    CHECK_VK_THROW(vkCreateSemaphore(device.device, tmpPtr((VkSemaphoreCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    }), nullptr, &image_acquired_semaphore));

    vk.setDebugUtilsObjectNameEXT(tmpPtr((VkDebugUtilsObjectNameInfoEXT) {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .objectType = VK_OBJECT_TYPE_SEMAPHORE,
        .objectHandle = reinterpret_cast<uint64_t>(image_acquired_semaphore),
        .pObjectName = "SwapchainSlot::image_acquired"
    }));

    VkFence fence;
    CHECK_VK_THROW(vkCreateFence(device.device, tmpPtr((VkFenceCreateInfo) {
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
            throw std::runtime_error("unhandled acquire error");
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

void Swapchain::resize() {
    _impl->should_resize = true;
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
