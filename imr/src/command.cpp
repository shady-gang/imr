#include "imr_private.h"

namespace imr {

CommandBuffer::CommandBuffer(imr::Device& device, imr::Queue& queue, VkCommandBufferLevel level, VkCommandBufferUsageFlags usage) {
    impl_ = std::make_unique<Impl>(*this, device, queue, level, usage, nullptr);
}

void CommandBuffer::addCleanupAction(std::function<void()>&& fn) {
    impl_->addCleanupAction(std::move(fn));
}

void CommandBuffer::submit(std::vector<VkSemaphore> waits, std::vector<VkSemaphore> signals) {
    impl_->submit(waits, signals);
}

CommandBuffer::~CommandBuffer() {
    impl_.reset();
}

CommandBuffer::Impl::Impl(CommandBuffer& parent, imr::Device& device, imr::Queue& queue, VkCommandBufferLevel level, VkCommandBufferUsageFlags usage, Pool* pool) : parent_(parent), device_(device), queue_(queue), pool_ptr_(pool) {
    if (!pool) {
        owned_pool_ = std::make_unique<Pool>(device, queue_.index);
        pool_ptr_ = &*owned_pool_;
    }

    CHECK_VK_THROW(vkAllocateCommandBuffers(device.device, tmpPtr<VkCommandBufferAllocateInfo>({
       .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
       .commandPool = pool_ptr_->handle_,
       .level = level,
       .commandBufferCount = 1,
    }), &parent_.handle));

    CHECK_VK_THROW(vkBeginCommandBuffer(parent.handle, tmpPtr<VkCommandBufferBeginInfo>({
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = usage,
    })));

    CHECK_VK_THROW(vkCreateFence(device.device, tmpPtr<VkFenceCreateInfo>({
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = 0,
    }), nullptr, &fence_));
}

void CommandBuffer::Impl::addCleanupAction(std::function<void()>&& fn) {
    cleanup_queue_.push_back(fn);
}

void CommandBuffer::Impl::submit(std::vector<VkSemaphore> waits, std::vector<VkSemaphore> signals) {
    vkEndCommandBuffer(parent_.handle);
    vkQueueSubmit(*queue_.handle.lock_mut(), 1, tmpPtr<VkSubmitInfo>({
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = (uint32_t) waits.size(),
        .pWaitSemaphores = waits.data(),
        .pWaitDstStageMask = tmpPtr((VkPipelineStageFlags) VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT),
        .commandBufferCount = 1,
        .pCommandBuffers = &parent_.handle,
        .signalSemaphoreCount = (uint32_t) signals.size(),
        .pSignalSemaphores = signals.data(),
    }), fence_);
}

CommandBuffer::Impl::~Impl() {
    vkWaitForFences(device_.device, 1, &fence_, true, UINT64_MAX);
    vkDestroyFence(device_.device, fence_, nullptr);

    while (cleanup_queue_.size() > 0) {
        cleanup_queue_.back()();
        cleanup_queue_.pop_back();
    }

    vkFreeCommandBuffers(device_.device, pool_ptr_->handle_, 1, &parent_.handle);

    owned_pool_ = nullptr;
}

}
