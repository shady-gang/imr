#ifndef IMR_PRIVATE_H
#define IMR_PRIVATE_H

#include "imr/imr.h"

#include "rustex.h"
#include "vk_mem_alloc.h"

#include <thread>

#define CHECK_VK_THROW(do) CHECK_VK(do, throw std::runtime_error(#do))

namespace imr {

struct Device::Impl {
    Device& public_;
    Impl(Device& p) : public_(p) {}

    VmaAllocator allocator;

    rustex::mutex<VkQueue> main_queue;
    uint32_t main_queue_idx;

    rustex::mutex<std::unordered_map<std::thread::id, VkCommandPool>> pools;

    //std::vector<std::unique_ptr<Buffer>> buffers;
    std::vector<std::unique_ptr<Image>> images;

    VkCommandPool get_pool_for_thread() {
        {
            auto pools = this->pools.lock();
            auto found = pools->find(std::this_thread::get_id());
            if (found != pools->end())
                return found->second;
        }

        VkCommandPool pool;
        CHECK_VK(vkCreateCommandPool(public_.device.device, tmpPtr<VkCommandPoolCreateInfo>({
             .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
             .queueFamilyIndex = main_queue_idx,
         }), nullptr, &pool), throw std::runtime_error("failed to create cmdpool"));

        auto pools = this->pools.lock_mut();
        (*pools)[std::this_thread::get_id()] = pool;
        return pool;
    }

    ~Impl() {
        auto pools = this->pools.lock_mut();
        for (auto [_, pool] : *pools) {
            vkDestroyCommandPool(public_.device.device, pool, nullptr);
        }
    }
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
