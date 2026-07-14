#include "swapchain_private.h"
#include "imr/util.h"

#include "VkBootstrap.h"

#include <functional>
#include <vector>
#include <algorithm>
#include <thread>
#include <chrono>

namespace imr {

Swapchain::Slot::Slot(std::unique_ptr<Impl>&& impl) {
    impl_ = std::move(impl);
}

Swapchain::Slot::~Slot() {}

Image& Swapchain::Slot::image() const {
    return *impl_->image;
}

void Swapchain::Slot::queuePresent(std::vector<VkSemaphore> waits) {
    return impl_->queuePresent(waits);
}

Swapchain::Slot::Impl::Impl(Swapchain& s) : device_(s._impl->device), swapchain(s) {
    auto& device = s._impl->device;
    auto& vk = device.dispatch;

    CHECK_VK_THROW(vkCreateSemaphore(device.device, tmpPtr<VkSemaphoreCreateInfo>({
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    }), nullptr, &present_semaphore));

    CHECK_VK_THROW(vk.setDebugUtilsObjectNameEXT(tmpPtr<VkDebugUtilsObjectNameInfoEXT>({
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .objectType = VK_OBJECT_TYPE_SEMAPHORE,
        .objectHandle = reinterpret_cast<uint64_t>(present_semaphore),
        .pObjectName = "SwapchainSlot::present_queued"
    })));
}

Swapchain::Slot::Impl::~Impl() {
    auto& device = swapchain._impl->device;
    if (present_fence) {
        CHECK_VK_THROW(vkWaitForFences(device.device, 1, &present_fence, true, UINT64_MAX));
        vkDestroyFence(device.device, present_fence, nullptr);
        present_fence = nullptr;
    }
    vkDestroySemaphore(device.device, present_semaphore, nullptr);
}

Swapchain::Swapchain(Device& device, GLFWwindow* window) {
    _impl = std::make_unique<Swapchain::Impl>(*this, device, window);
    _impl->build_swapchain();
}

void Swapchain::beginFrame(std::function<void(Swapchain::Frame&)>&& fn) {
    Frame& frame = _impl->begin_frame();
    fn(frame);
}

Device& Swapchain::device() const { return _impl->device; }

VkFormat Swapchain::format() const {
    return _impl->swapchain.image_format;
}

void Swapchain::resize() {
    _impl->should_resize = true;
}

void Swapchain::drain() {
    _impl->drain();
}

Swapchain::~Swapchain() {}

Swapchain::Impl::Impl(Swapchain& parent, Device& device, GLFWwindow* window) : parent(parent), device(device), window(window) {
    CHECK_VK_THROW(glfwCreateWindowSurface(device.context.instance, window, nullptr, &surface));

    frames_in_flight.resize(3);
}

void Swapchain::Impl::build_swapchain() {
    uint32_t surface_formats_count;
    CHECK_VK_THROW(vkGetPhysicalDeviceSurfaceFormatsKHR(device.physical_device, surface, &surface_formats_count, nullptr));

    std::vector<VkSurfaceFormatKHR> formats;
    formats.resize(surface_formats_count);
    CHECK_VK_THROW(vkGetPhysicalDeviceSurfaceFormatsKHR(device.physical_device, surface, &surface_formats_count, formats.data()));

    std::optional<VkSurfaceFormatKHR> preferred;
    for (auto format : formats) {
        if (format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            if (format.format == VK_FORMAT_R8G8B8A8_UNORM)
                preferred = format;
            if (format.format == VK_FORMAT_B8G8R8A8_UNORM)
                preferred = format;
        }
    }

    if (!preferred) {
        fprintf(stderr, "Swapchain format is not 8-bit RGBA or BGRA");
    }

    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

    auto builder = vkb::SwapchainBuilder(device.physical_device, device.device, surface);
    builder.add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT);
    builder.set_desired_extent(width, height);
    builder.set_desired_present_mode(VK_PRESENT_MODE_MAILBOX_KHR);
    builder.add_fallback_present_mode(VK_PRESENT_MODE_IMMEDIATE_KHR);
    if (preferred)
        builder.set_desired_format(*preferred);

    if (auto built = builder.build(); built.has_value()) {
        swapchain = built.value();
    } else {
        fprintf(stderr, "Failed to build a swapchain (size=%d,%d, error=%d).\n", width, height, built.vk_result());
        throw std::runtime_error("failure to build a swapchain");
    }

    for (int i = 0; i < swapchain.image_count; i++) {
        slots.emplace_back(std::make_unique<Swapchain::Slot>(std::make_unique<Swapchain::Slot::Impl>(parent)));
    }
}

void Swapchain::Impl::destroy_swapchain() {
    slots.clear();
    vkb::destroy_swapchain(swapchain);
}

/// Acquires the next image
std::optional<std::tuple<Swapchain::Slot&, VkSemaphore, VkFence>> Swapchain::Impl::try_acquire_slot() {
    //auto& device = device;
    auto& vk = device.dispatch;

    uint32_t image_index;

    VkSemaphore image_acquired_semaphore;
    CHECK_VK_THROW(vkCreateSemaphore(device.device, tmpPtr<VkSemaphoreCreateInfo>({
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    }), nullptr, &image_acquired_semaphore));

    vk.setDebugUtilsObjectNameEXT(tmpPtr<VkDebugUtilsObjectNameInfoEXT>({
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .objectType = VK_OBJECT_TYPE_SEMAPHORE,
        .objectHandle = reinterpret_cast<uint64_t>(image_acquired_semaphore),
        .pObjectName = "SwapchainSlot::image_acquired"
    }));

    VkFence acquired_fence;
    CHECK_VK_THROW(vkCreateFence(device.device, tmpPtr<VkFenceCreateInfo>({
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
    }), nullptr, &acquired_fence));

    VkResult acquire_result = device.dispatch.acquireNextImageKHR(swapchain, UINT64_MAX, image_acquired_semaphore, acquired_fence, &image_index);
    switch (acquire_result) {
        case VK_SUCCESS: break;
        case VK_SUBOPTIMAL_KHR: should_resize = true; break;
        case VK_ERROR_OUT_OF_DATE_KHR: {
            fprintf(stderr, "Acquire failed. We need to resize!\n");
            vkDestroySemaphore(device.device, image_acquired_semaphore, nullptr);
            vkDestroyFence(device.device, acquired_fence, nullptr);
            return std::nullopt;
        }
        default:
            fprintf(stderr, "Acquire result was: %d\n", acquire_result);
            throw std::runtime_error("unhandled acquire error");
    }

    // We know the next image !
    Swapchain::Slot& slot = *slots[image_index];
    //printf("Image acquired: %d\n", image_index);

    VkImage bare_image = swapchain.get_images().value()[image_index];
    auto vkb_swapchain = slot.impl_->swapchain._impl->swapchain;
    VkExtent3D size = { vkb_swapchain.extent.width, vkb_swapchain.extent.height, 1 };
    auto i = make_image_from(device, bare_image, VK_IMAGE_TYPE_2D, size, vkb_swapchain.image_format);
    slot.impl_->image = std::make_unique<Image>(std::move(i));
    slot.impl_->image_index = image_index;

    VkSemaphore prev_acquire = slot.impl_->acquire_semaphore;

    VkFence wait_fence = VK_NULL_HANDLE;
    // TODO: use swapchain maintenance fence optionally
    if (prev_acquire != VK_NULL_HANDLE) {
        wait_fence = acquired_fence;
    }
    if (wait_fence) {
        //assert(slot.impl_->frame);
        //slot.impl_->frame->reset();
        CHECK_VK_THROW(vkWaitForFences(device.device, 1, &wait_fence, true, UINT64_MAX));
        vkDestroyFence(device.device, wait_fence, nullptr);
        vkDestroySemaphore(device.device, prev_acquire, nullptr);
    }

    slot.impl_->acquire_semaphore = image_acquired_semaphore;
    slot.waits = { image_acquired_semaphore };

    //printf("Waited for %llx\n", (uint64_t) slot.wait_for_previous_present);
    //slot.impl_->frame.reset();

    return std::tie(slot, image_acquired_semaphore, acquired_fence);
}

std::tuple<Swapchain::Slot&, VkSemaphore, VkFence> Swapchain::Impl::acquire_slot() {
    while (true) {
        if (should_resize) {
            should_resize = false;
            glfwPollEvents();
            drain();
            destroy_swapchain();
            build_swapchain();
        }
        auto result = try_acquire_slot();
        if (!result) {
            should_resize = true;
            continue;
        }
        return *result;
    }
}

void Swapchain::Slot::Impl::queuePresent(std::vector<VkSemaphore> waits) {
    auto& slot = *this;
    auto& swapchain = slot.swapchain;
    auto& device = device_;

    // assert(!submitted && "Cannot submit a frame twice!");
    // submitted = true;

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

    // std::vector<VkSemaphore> semaphores;
    // semaphores.push_back(slot.present_semaphore);

    VkSwapchainPresentFenceInfoKHR present_fence_info {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_FENCE_INFO_KHR,
        .swapchainCount = 1,
        .pFences = &present_fence,
    };

    auto lock = device._impl->main_queue.handle.lock_mut();
    VkResult present_result = vkQueuePresentKHR(*lock, tmpPtr<VkPresentInfoKHR>({
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext = present_fence != VK_NULL_HANDLE ? &present_fence_info : nullptr,
        .waitSemaphoreCount = static_cast<uint32_t>(waits.size()),
        .pWaitSemaphores = waits.data(),
        .swapchainCount = 1,
        .pSwapchains = &swapchain._impl->swapchain.swapchain,
        .pImageIndices = &slot.image_index,
    }));
    //printf("Queued presentation, will signal %llx\n", (uint64_t) slot.wait_for_previous_present);
    switch (present_result) {
        case VK_SUCCESS:
        case VK_SUBOPTIMAL_KHR: break;
        case VK_ERROR_OUT_OF_DATE_KHR: {
            fprintf(stderr, "Present failed. We need to resize!\n");
            break;
        }
        default: throw std::runtime_error("unhandled queuePresent result");
    }
}

void Swapchain::Impl::drain() {
    vkDeviceWaitIdle(device.device);

    for (auto& frame : frames_in_flight) {
        if (frame)
            frame.reset();
    }
}

Swapchain::Impl::~Impl() {
    drain();
    destroy_swapchain();
    vkDestroySurfaceKHR(device.context.dispatch.instance, surface, nullptr);
}

}
