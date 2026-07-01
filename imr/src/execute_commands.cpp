#include "imr_private.h"

namespace imr {

std::function<void(void)> Device::executeCommandsAsync(std::function<void(VkCommandBuffer)> lambda) {
    VkCommandBuffer cmdbuf;
    auto pool = _impl->get_pool_for_thread();
    CHECK_VK_THROW(vkAllocateCommandBuffers(device.device, tmpPtr<VkCommandBufferAllocateInfo>({
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    }), &cmdbuf));

    vkBeginCommandBuffer(cmdbuf, tmpPtr<VkCommandBufferBeginInfo>({
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    }));

    lambda(cmdbuf);

    VkFence fence;
    auto d = device.device;
    vkCreateFence(device.device, tmpPtr<VkFenceCreateInfo>({
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = 0,
    }), nullptr, &fence);

    vkEndCommandBuffer(cmdbuf);

    {
        auto q = _impl->main_queue.lock_mut();
        vkQueueSubmit(*q, 1, tmpPtr<VkSubmitInfo>({
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .waitSemaphoreCount = 0,
            .pWaitSemaphores = nullptr,
            .pWaitDstStageMask = tmpPtr((VkPipelineStageFlags) VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT),
            .commandBufferCount = 1,
            .pCommandBuffers = &cmdbuf,
            .signalSemaphoreCount = 0,
            .pSignalSemaphores = nullptr,
        }), fence);
    }

    auto wait = [=, this]() { ;
        vkWaitForFences(d, 1, &fence, true, UINT64_MAX);
        vkDestroyFence(d, fence, nullptr);

        auto pool2 = _impl->get_pool_for_thread();
        assert(pool == pool2);
        vkFreeCommandBuffers(d, pool, 1, &cmdbuf);
    };
    return wait;
}

void Device::executeCommandsSync(std::function<void(VkCommandBuffer)> lambda) {
    executeCommandsAsync(lambda)();
}


}