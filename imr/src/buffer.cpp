#include "imr_private.h"

namespace imr {

struct Buffer::Impl {
    Device& device;
    VkBufferUsageFlags usage;
    VkMemoryPropertyFlags memory_properties;

    VmaAllocation allocation;
    VmaAllocationInfo allocation_info;

    size_t size;
    VkBuffer handle;

    std::optional<VkDeviceAddress> device_address;

    /// Managed by the allocator, required for mapping the buffer
    VkDeviceMemory memory;
    size_t memory_offset;

    Impl(Device& device, VkBufferUsageFlags usage, VkMemoryPropertyFlags memory_properties, size_t size) : device(device), usage(usage), memory_properties(memory_properties) {
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
            .requiredFlags = memory_properties,
        };
        CHECK_VK(vmaCreateBuffer(device._impl->allocator, &buffer_ci, &vma_aci, &handle, &allocation, &allocation_info), throw std::exception());
        memory = allocation_info.deviceMemory;
        memory_offset = allocation_info.offset;
    }

    VkDeviceAddress get_device_address() {
        if (device_address)
            return *device_address;
        device_address = vkGetBufferDeviceAddress(device.device, tmpPtr<VkBufferDeviceAddressInfo>({
            .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
            .buffer = handle,
        }));
        return *device_address;
    }
};

Buffer::Buffer(imr::Device& device, size_t size, VkBufferUsageFlags usage, VkMemoryPropertyFlags memory_property) : size(size) {
    _impl = std::make_unique<Impl>(device, usage, memory_property, size);
    handle = _impl->handle;
}

VkDeviceAddress Buffer::device_address() {
    return _impl->get_device_address();
}

void Buffer::uploadDataSync(uint64_t offset, uint64_t size, void* data) {
    auto& device = _impl->device;
    if (_impl->memory_properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
        void* mapped_buffer;
        CHECK_VK_THROW(vmaMapMemory(device._impl->allocator, _impl->allocation, &mapped_buffer));
        memcpy(mapped_buffer, data, size);
        vmaUnmapMemory(device._impl->allocator, _impl->allocation);
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
