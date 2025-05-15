#ifndef IMR_PRIVATE_H
#define IMR_PRIVATE_H

#include "imr/imr.h"

#include "vk_mem_alloc.h"

#define CHECK_VK_THROW(do) CHECK_VK(do, throw std::runtime_error(#do))

namespace imr {

struct Device::Impl {
    VmaAllocator allocator;

    //std::vector<std::unique_ptr<Buffer>> buffers;
    std::vector<std::unique_ptr<Image>> images;
};

Image make_image_from(Device& device, VkImage existing_handle, VkImageType dim, VkExtent3D size, VkFormat format);

}

#endif
