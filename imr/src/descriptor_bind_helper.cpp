#include "shader_private.h"

namespace imr {

struct DescriptorBindHelper::Impl {
    Device& device;
    PipelineLayout& layout;
    ReflectedLayout& reflected;
    VkPipelineBindPoint bind_point;

    unsigned nsets;
    VkDescriptorSet* sets;
    VkDescriptorPool pool;

    std::vector<std::function<void(void)>> cleanup;
    bool committed = false;

    Impl(Device& device, PipelineLayout& layout, ReflectedLayout& reflected, VkPipelineBindPoint bind_point) : device(device), layout(layout), reflected(reflected), bind_point(bind_point) {
        auto& vk = device.dispatch;
        nsets = reflected.set_bindings.size();

        std::unordered_map<VkDescriptorType, uint32_t> descriptor_counts;
        auto access_map = [&](VkDescriptorType key) -> uint32_t& {
            if (descriptor_counts.contains(key))
                return descriptor_counts[key];
            return descriptor_counts[key] = 0;
        };
        for (auto& [set, bindings] : reflected.set_bindings) {
            for (auto& binding : bindings) {
                access_map(binding.descriptorType) += binding.descriptorCount;
            }
        }

        std::vector<VkDescriptorPoolSize> pool_sizes;
        for (auto& [type, count] : descriptor_counts) {
            VkDescriptorPoolSize size = {
                .type = type,
                .descriptorCount = count,
            };
            pool_sizes.push_back(size);
        }

        vkCreateDescriptorPool(vk.device, tmpPtr((VkDescriptorPoolCreateInfo) {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
            .maxSets = static_cast<uint32_t>(layout.set_layouts.size()),
            .poolSizeCount = static_cast<uint32_t>(pool_sizes.size()),
            .pPoolSizes = pool_sizes.data(),
        }), nullptr, &pool);

        sets = reinterpret_cast<VkDescriptorSet*>(calloc(nsets, sizeof(VkDescriptorSet)));
    }

    // Lazily allocates the set if we need it
    VkDescriptorSet get_or_create_set(unsigned set) {
        if (sets[set] == 0) {
            CHECK_VK_THROW(vkAllocateDescriptorSets(device.device, tmpPtr((VkDescriptorSetAllocateInfo) {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                .descriptorPool = pool,
                .descriptorSetCount = 1,
                .pSetLayouts = &layout.set_layouts[set],
            }), &sets[set]));
        }
        return sets[set];
    }

    ~Impl() {
        free(sets);
        vkDestroyDescriptorPool(device.device, pool, nullptr);

        for (auto& fn : cleanup) {
            fn();
        }
    }
};

DescriptorBindHelper::DescriptorBindHelper(std::unique_ptr<imr::DescriptorBindHelper::Impl>&& impl) {
    _impl = std::move(impl);
}

DescriptorBindHelper::~DescriptorBindHelper() {}

DescriptorBindHelper* ComputeShader::create_bind_helper() {
    auto impl = std::make_unique<DescriptorBindHelper::Impl>(_impl->device, *_impl->layout, *_impl->reflected, VK_PIPELINE_BIND_POINT_COMPUTE);
    return new DescriptorBindHelper(std::move(impl));
}

VkImageViewType image_type_to_view_type(VkImageType type) {
    switch (type) {
        case VK_IMAGE_TYPE_1D: return VK_IMAGE_VIEW_TYPE_1D;
        case VK_IMAGE_TYPE_2D: return VK_IMAGE_VIEW_TYPE_2D;
        case VK_IMAGE_TYPE_3D: return VK_IMAGE_VIEW_TYPE_3D;
        default: throw std::runtime_error("Unknown image type");
    }
}

void DescriptorBindHelper::set_storage_image(uint32_t set, uint32_t binding, Image& image, std::optional<VkImageSubresourceRange> subresource, std::optional<VkImageViewType> image_view_type) {
    assert(!_impl->committed);
    auto& device = _impl->device;

    VkImageViewType final_image_view_type = image_view_type ? *image_view_type : image_type_to_view_type(image.type());
    VkImageSubresourceRange subresource_range = subresource ? *subresource : image.whole_image_subresource_range();

    VkImageView view;
    vkCreateImageView(device.device, tmpPtr((VkImageViewCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = image.handle(),
        .viewType = final_image_view_type,
        .format = image.format(),
        .subresourceRange = subresource_range,
    }), nullptr, &view);

    vkUpdateDescriptorSets(device.device, 1, tmpPtr((VkWriteDescriptorSet) {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = _impl->get_or_create_set(set),
        .dstBinding = binding,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        .pImageInfo = tmpPtr((VkDescriptorImageInfo) {
            .sampler = VK_NULL_HANDLE,
            .imageView = view,
            .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
        }),
    }), 0, nullptr);

    auto deviceHandle = device.device.device;
    _impl->cleanup.push_back([=]() {
        vkDestroyImageView(deviceHandle, view, nullptr);
    });
}

void DescriptorBindHelper::commit(VkCommandBuffer cmdbuf) {
    assert(!_impl->committed);
    for (unsigned set = 0; set < _impl->nsets; set++) {
        if (_impl->sets[set])
            vkCmdBindDescriptorSets(cmdbuf, _impl->bind_point, _impl->layout.pipeline_layout, set, 1, &_impl->sets[set], 0, nullptr);
    }
    _impl->committed = true;
}

}