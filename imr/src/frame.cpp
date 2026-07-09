#include "swapchain_private.h"
#include "imr/util.h"

#include <chrono>
#include <thread>

namespace imr {

void Swapchain::Frame::addCleanupAction(std::function<void(void)>&& fn) {
    _impl->cleanup_queue.push_back(std::move(fn));
}

Swapchain::Frame::Frame(std::unique_ptr<Impl>&& impl) {
    _impl = std::move(impl);
    id = _impl->id_;
}

Swapchain::Slot& Swapchain::Frame::slot() const { return _impl->slot; }

Swapchain::Frame::~Frame() {}

Swapchain::Frame::Impl::Impl(Device& device, Swapchain::Slot& slot, size_t id) : device(device), slot(slot), id_(id) {
    auto& vk = device.dispatch;

    CHECK_VK_THROW(vkCreateSemaphore(device.device, tmpPtr<VkSemaphoreCreateInfo>({
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    }), nullptr, &frame_finished));

    std::string frame_semaphore_name = "done[" + std::to_string(id) + "]";
    CHECK_VK_THROW(vk.setDebugUtilsObjectNameEXT(tmpPtr<VkDebugUtilsObjectNameInfoEXT>({
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .objectType = VK_OBJECT_TYPE_SEMAPHORE,
        .objectHandle = reinterpret_cast<uint64_t>(frame_finished),
        .pObjectName = frame_semaphore_name.c_str()
    })));

    // if someone was waiting on us, they'll delete this
    cleanup_queue.emplace_back([&]() {
        if (frame_finished != VK_NULL_HANDLE)
            vkDestroySemaphore(device.device, frame_finished, nullptr);
    });
}

Swapchain::Frame::Impl::~Impl() {
    for (auto fence : recycle_waits) {
        CHECK_VK_THROW(vkWaitForFences(device.device, 1, &fence, true, UINT64_MAX));
    }

    // We want to iterate over the queue in a FIFO manner
    while (cleanup_queue.size() > 0) {
        cleanup_queue.back()();
        cleanup_queue.pop_back();
    }
}

Swapchain::Frame& Swapchain::Impl::begin_frame() {
    std::unique_ptr<Swapchain::Frame>& cell = frames_in_flight[next_fif];
    size_t fif = next_fif;
    size_t prev_fif = (fif + frames_in_flight.size() - 1) % frames_in_flight.size();
    next_fif = (fif + 1) % frames_in_flight.size();

    if (cell)
        cell.reset();

    auto [slot, acquired_semaphore, acquired_fence] = acquire_slot();

    auto& frame = *(cell = std::make_unique<Frame>(std::move(std::make_unique<Frame::Impl>(device, slot, frame_counter++))));
    frame.signals.push_back(frame._impl->frame_finished);

    if (fif != prev_fif && frames_in_flight[prev_fif]) {
        Frame& prev_frame = *frames_in_flight[prev_fif];
        frame.waits.push_back(prev_frame._impl->frame_finished);
        // we take over the responsibility of destroying the semaphore since we now wait on it
        frame.addCleanupAction([&, sem = prev_frame._impl->frame_finished](){
            vkDestroySemaphore(device.device, sem, nullptr);
        });
        prev_frame._impl->frame_finished = VK_NULL_HANDLE;
    }

    // slot.impl_->frame = &cell;
    return frame;
}

VkSemaphore Swapchain::Frame::Impl::create_frame_lived_semaphore(std::string name) {
    //return slot.impl_->present_semaphore;

    auto& vk = this->device.dispatch;

    VkSemaphore semaphore;
    CHECK_VK_THROW(vkCreateSemaphore(device.device, tmpPtr<VkSemaphoreCreateInfo>({
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    }), nullptr, &semaphore));

    std::string frame_semaphore_name = name + "[" + std::to_string(id_) + "]";
    CHECK_VK_THROW(vk.setDebugUtilsObjectNameEXT(tmpPtr<VkDebugUtilsObjectNameInfoEXT>({
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .objectType = VK_OBJECT_TYPE_SEMAPHORE,
        .objectHandle = reinterpret_cast<uint64_t>(semaphore),
        .pObjectName = frame_semaphore_name.c_str()
    })));

    cleanup_queue.emplace_back([semaphore, device = device.device.device]() {
        vkDestroySemaphore(device, semaphore, nullptr);
    });

    return semaphore;
}

}
