#include "imr_private.h"

namespace imr {

std::function<void(void)> Device::executeCommandsAsync(std::function<void(VkCommandBuffer)> lambda) {
    auto cmd = std::make_shared<imr::CommandBuffer>(*this, _impl->main_queue);

    lambda(*cmd);

    cmd->submit();

    std::function<void(void)> wait = [cmd]() { ;

    };
    return wait;
}

void Device::executeCommandsSync(std::function<void(VkCommandBuffer)> lambda) {
    auto cmd = std::make_unique<imr::CommandBuffer>(*this, _impl->asyn_queue);
    lambda(*cmd);
    cmd->submit();
}


}