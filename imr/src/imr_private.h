#ifndef IMR_PRIVATE_H
#define IMR_PRIVATE_H

#include "imr/imr.h"

#include "rustex.h"
#include "vk_mem_alloc.h"

#include <thread>

#define CHECK_VK_THROW(do) CHECK_VK(do, throw std::runtime_error(#do))

namespace imr {

struct Queue {
    uint32_t index;
    rustex::mutex<VkQueue> handle;

    Queue(imr::Device& device, vkb::QueueType type) {
        index = device.device.get_queue_index(type).value();
        *handle.lock_mut() = device.device.get_queue(type).value();
    }

    Queue() = delete;
};

struct Device::Impl {
    Device& public_;
    Impl(Device& p) : public_(p), main_queue(p, vkb::QueueType((int) vkb::QueueType::graphics | (int) vkb::QueueType::present)), asyn_queue(p, vkb::QueueType::transfer) {}

    VmaAllocator allocator;

    Queue main_queue;
    Queue asyn_queue;

    //std::vector<std::unique_ptr<Buffer>> buffers;
    std::vector<std::unique_ptr<Image>> images;

    ~Impl() {
    }
};

struct Pool {
    imr::Device& device_;
    VkCommandPool handle_ = VK_NULL_HANDLE;

    Pool(imr::Device& device, Queue& queue) : device_(device) {
        CHECK_VK(vkCreateCommandPool(device.device, tmpPtr<VkCommandPoolCreateInfo>({
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .queueFamilyIndex = queue.index,
        }), nullptr, &handle_), throw std::runtime_error("failed to create cmdpool"));
    }

    ~Pool() {
        vkDestroyCommandPool(device_.device, handle_, nullptr);
    }
};

struct CommandBuffer::Impl {
    CommandBuffer& parent_;
    imr::Device& device_;
    Queue& queue_;
    Pool* pool_ptr_;
    std::unique_ptr<Pool> owned_pool_;

    VkFence fence_ = VK_NULL_HANDLE;
    std::vector<std::function<void(void)>> cleanup_queue_;

    Impl(CommandBuffer& parent, imr::Device& device, imr::Queue& queue, VkCommandBufferLevel level, VkCommandBufferUsageFlags usage, Pool* pool);
    void addCleanupAction(std::function<void()>&& fn);
    void submit(std::vector<VkSemaphore> waits = {  }, std::vector<VkSemaphore> signals = {  });
    ~Impl();
};

VkCommandBuffer alloc_cmdbuf(Device& device);

static inline void appendPNext(VkBaseOutStructure* base, VkBaseOutStructure* ext) {
    while (base->pNext) {
        base = base->pNext;
    }
    base->pNext = ext;
}

Image make_image_from(Device& device, VkImage existing_handle, VkImageType dim, VkExtent3D size, VkFormat format);

}

#endif
