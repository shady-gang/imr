#include "imr_private.h"

namespace imr {

static auto make_default_device_selector(Context& context) {
    auto device_selector = vkb::PhysicalDeviceSelector(context.instance)
        .add_required_extension("VK_KHR_maintenance2")
        .add_required_extension("VK_KHR_create_renderpass2")
        .add_required_extension("VK_KHR_dynamic_rendering")
        .add_required_extension("VK_KHR_synchronization2")
        //.add_required_extension("VK_KHR_swapchain_maintenance1")
        .add_required_extension(VK_EXT_SUBGROUP_SIZE_CONTROL_EXTENSION_NAME)
        .add_required_extension(VK_EXT_SCALAR_BLOCK_LAYOUT_EXTENSION_NAME)
        .set_minimum_version(1, 2)
        .set_required_features(VkPhysicalDeviceFeatures({
            .shaderUniformBufferArrayDynamicIndexing = true,
            .shaderInt64 = true,
            .shaderInt16 = true,
        }))
        .defer_surface_initialization()
        .add_required_extension_features(VkPhysicalDeviceScalarBlockLayoutFeatures({
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES,
            .scalarBlockLayout = true,
        }))
        .add_required_extension_features(VkPhysicalDeviceBufferDeviceAddressFeatures({
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES,
            .bufferDeviceAddress = true,
        }))
        .add_required_extension_features(VkPhysicalDeviceSynchronization2FeaturesKHR({
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR,
            .pNext = nullptr,
            .synchronization2 = true,
        }))
        .add_required_extension_features(VkPhysicalDeviceDynamicRenderingFeaturesKHR({
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR,
                .dynamicRendering = VK_TRUE
        }))
        .add_required_extension_features((VkPhysicalDeviceFloat16Int8FeaturesKHR) {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FLOAT16_INT8_FEATURES_KHR,
            .shaderInt8 = true,
        })
        .add_required_extension_features(VkPhysicalDeviceMeshShaderFeaturesEXT({
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT,
                .taskShader = VK_TRUE,
                .meshShader = VK_TRUE,
        }))
        .add_required_extension_features(VkPhysicalDeviceShaderMaximalReconvergenceFeaturesKHR({
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_MAXIMAL_RECONVERGENCE_FEATURES_KHR,
                .shaderMaximalReconvergence = true,
        }))
        .add_required_extension_features(VkPhysicalDeviceSubgroupSizeControlFeatures({
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_FEATURES,
                .subgroupSizeControl = true,
        }));
    return device_selector;
}

std::vector<vkb::PhysicalDevice> Context::available_devices(std::function<void(vkb::PhysicalDeviceSelector&)>&& f) {
    auto selector = make_default_device_selector(*this);
    f(selector);
    return selector.select_devices().value();
}

Device::Device(Context& context, std::function<void(vkb::PhysicalDeviceSelector&)>&& device_custom) : Device(context, ([&]() -> vkb::PhysicalDevice {
    auto device_selector = make_default_device_selector(context);

    device_custom(device_selector);

    auto selected = device_selector.select();
    if (selected.has_value())
        return selected.value();
    else
        throw std::runtime_error("failed to select a device");
})()) {}

Device::Device(imr::Context& context, vkb::PhysicalDevice physical_device) : context(context), physical_device(physical_device) {
    if (auto built = vkb::DeviceBuilder(physical_device)
            .build(); built.has_value())
    {
        device = built.value();
        dispatch = device.make_table();
    }

    _impl = std::make_unique<Impl>(*this);

    CHECK_VK(vmaCreateAllocator(tmpPtr<VmaAllocatorCreateInfo>({
        .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
        .physicalDevice = physical_device,
        .device = device,
        .instance = context.instance,
    }), &_impl->allocator), throw std::runtime_error("failed to create VMA allocator"));
}

Device::~Device() {
    vkDeviceWaitIdle(device);

    vmaDestroyAllocator(_impl->allocator);
    //vkDestroyCommandPool(device, *_impl->pool.lock_mut(), nullptr);
    _impl.reset();
    vkb::destroy_device(device);
}

Queue& Device::main_queue() {
    return _impl->main_queue;
}

}
