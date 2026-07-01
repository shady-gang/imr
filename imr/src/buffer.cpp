#include "imr_private.h"

namespace imr {

struct Buffer::Impl {
    Device& device;
    VkBufferUsageFlags usage;
    VkMemoryPropertyFlags memory_property;

    VmaAllocation allocation;
    VmaAllocationInfo allocation_info;
};

Buffer::Buffer(imr::Device& device, size_t size, VkBufferUsageFlags usage, VkMemoryPropertyFlags memory_property) : size(size) {
    _impl = std::make_unique<Impl>(device, usage, memory_property);
    VkBufferCreateInfo buffer_ci = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .flags = 0,
        .size = size,
        .usage = usage | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    VmaAllocationCreateInfo vma_aci = {
        .flags = 0,
        .usage = VMA_MEMORY_USAGE_UNKNOWN,
        .requiredFlags = memory_property
    };
    CHECK_VK(vmaCreateBuffer(device._impl->allocator, &buffer_ci, &vma_aci, &handle, &_impl->allocation, &_impl->allocation_info), throw std::exception());
    memory = _impl->allocation_info.deviceMemory;
    memory_offset = _impl->allocation_info.offset;
}

VkDeviceAddress Buffer::device_address() {
    return vkGetBufferDeviceAddress(_impl->device.device, tmpPtr<VkBufferDeviceAddressInfo>({
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = handle,
    }));
}

void Buffer::uploadDataSync(uint64_t offset, uint64_t size, void* data) {
    auto& device = _impl->device;
    if (_impl->memory_property & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
        void* mapped_buffer;
        CHECK_VK_THROW(vmaMapMemory(device._impl->allocator, _impl->allocation, &mapped_buffer));
        //global_mem_mtx.lock();
        //CHECK_VK_THROW(vkMapMemory(device.device, memory, memory_offset, size, 0, (void**) &mapped_buffer));
        //global_mem_mtx.unlock();
        memcpy(mapped_buffer, data, size);
        vmaUnmapMemory(device._impl->allocator, _impl->allocation);
        //global_mem_mtx.lock();
        //vkUnmapMemory(device.device, memory);
        //global_mem_mtx.unlock();
    } else if (_impl->usage & VK_BUFFER_USAGE_TRANSFER_DST_BIT) {
        // TODO: be less ridiculous, import host memory
        auto staging = imr::Buffer(device, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        staging.uploadDataSync(0, size, data);

        device.executeCommandsSync([&](VkCommandBuffer cmdbuf) {
            vkCmdCopyBuffer2(cmdbuf, tmpPtr<VkCopyBufferInfo2>({
                .sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
                .srcBuffer = staging.handle,
                .dstBuffer = handle,
                .regionCount = 1,
                .pRegions = tmpPtr<VkBufferCopy2>({
                    .sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
                    .srcOffset = 0,
                    .dstOffset = offset,
                    .size = size,
                })
            }));
        });
    } else {
        throw std::runtime_error("Error: This buffer was allocated without VK_BUFFER_USAGE_TRANSFER_DST_BIT or VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, we cannot do a host->GPU copy to it!");
    }
}

Buffer::~Buffer() {
    vmaDestroyBuffer(_impl->device._impl->allocator, handle, _impl->allocation);
}

}
