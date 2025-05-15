#include "imr_private.h"

namespace imr {

Context::Context(std::function<void(vkb::InstanceBuilder&)>&& instance_custom) {
    auto instance_builder = vkb::InstanceBuilder()
        .use_default_debug_messenger()
        .request_validation_layers()
        .set_minimum_instance_version(1, 3, 0)
        .enable_extension("VK_KHR_get_surface_capabilities2")
        //.enable_extension("VK_EXT_surface_maintenance1")
        .require_api_version(1, 3, 0);

    instance_custom(instance_builder);

    if (auto built = instance_builder
        .build(); built.has_value())
    {
        instance = built.value();
        dispatch = instance.make_table();
    } else {
        printf("%s\n", built.error().message().c_str());
        throw std::runtime_error("failed to build instance");
    }
}

Context::~Context() {
    vkb::destroy_instance(instance);
}

}
