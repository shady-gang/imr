#include "swapchain_private.h"
#include "imr/util.h"

#include <chrono>
#include <thread>

namespace imr {

void Swapchain::Frame::addCleanupFence(VkFence fence) {
    _impl->cleanup_fences.push_back(fence);
}

void Swapchain::Frame::addCleanupAction(std::function<void(void)>&& fn) {
    _impl->cleanup_queue.push_back(std::move(fn));
}

Swapchain::Frame::Frame(Impl&& impl) {
    _impl = std::make_unique<Frame::Impl>(std::move(impl));
}

Swapchain::Frame::Impl::Impl(Device& device, SwapchainSlot& slot) : device(device), slot(slot) {
    auto vkb_swapchain = slot.swapchain._impl->swapchain;
    VkExtent3D size = { vkb_swapchain.extent.width, vkb_swapchain.extent.height, 1 };
    auto i = make_image_from(device, slot.image, VK_IMAGE_TYPE_2D, size, vkb_swapchain.image_format);
    image = std::make_unique<Image>(std::move(i));
}

Image& Swapchain::Frame::image() const { return *_impl->image; }

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

void Swapchain::Frame::queuePresent() {
    assert(!_impl->queued);
    _impl->queued = true;
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
    semaphores.push_back(slot.present_semaphore);

    VkResult present_result = vkQueuePresentKHR(device.main_queue, tmpPtr((VkPresentInfoKHR) {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = static_cast<uint32_t>(semaphores.size()),
        .pWaitSemaphores = semaphores.data(),
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
        slot.frame = std::make_unique<Frame>(std::move(Frame::Impl(device, slot)));
        slot.frame->swapchain_image_available = acquired;
        slot.frame->signal_when_ready = slot.present_semaphore;
        slot.frame->id = _impl->frame_counter++;
        assert(acquired);
        slot.frame->addCleanupAction([=, &device]() {
            vkDestroySemaphore(device.device, acquired, nullptr);
        });

        //printf("Preparing frame: %d\n", slot.frame->id);
        fn(*slot.frame);
        break;
    }
}

}
