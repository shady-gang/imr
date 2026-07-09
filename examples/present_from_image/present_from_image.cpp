#include "imr/imr.h"
#include "imr/util.h"

#include "VkBootstrap.h"

#include <ctime>
#include <memory>
#include <filesystem>

struct vec2 { float x, y; };

struct Tri { vec2 v0, v1, v2; };

int main() {
    imr::Context context;
    imr::Device device(context);
    auto& vk = device.dispatch;

    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    auto window = glfwCreateWindow(1024, 1024, "Example", nullptr, nullptr);
    imr::Swapchain swapchain(device, window);

    imr::FpsCounter fps_counter;

    imr::ComputePipeline shader(device, "present_from_image.spv");

    VkDescriptorPool pool;
    vkCreateDescriptorPool(vk.device, tmpPtr((VkDescriptorPoolCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = 256,
        .poolSizeCount = 1,
        .pPoolSizes = tmpPtr((VkDescriptorPoolSize) {
            .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .descriptorCount = 256,
        }),
    }), nullptr, &pool);

    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    VkExtent3D extents = { static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1};
    auto image = new imr::Image (device, VK_IMAGE_TYPE_2D, extents, VK_FORMAT_R8G8B8A8_UNORM, (VkImageUsageFlagBits) (VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT));

    VkImageView view;
    vkCreateImageView(device.device, tmpPtr((VkImageViewCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = image->handle(),
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = image->format(),
        .subresourceRange = image->whole_image_subresource_range()
    }), nullptr, &view);

    VkDescriptorSet set;
    vkAllocateDescriptorSets(device.device, tmpPtr((VkDescriptorSetAllocateInfo) {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = pool,
        .descriptorSetCount = 1,
        .pSetLayouts = tmpPtr(shader.set_layout(0)),
    }), &set);

    vkUpdateDescriptorSets(device.device, 1, tmpPtr((VkWriteDescriptorSet) {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = set,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        .pImageInfo = tmpPtr((VkDescriptorImageInfo) {
            .sampler = VK_NULL_HANDLE,
            .imageView = view,
            .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
        }),
    }), 0, nullptr);

    while (!glfwWindowShouldClose(window)) {
        uint64_t now = imr_get_time_nano();
        fps_counter.tick();
        fps_counter.updateGlfwWindowTitle(window);

        swapchain.beginFrame([&](auto& frame) {
            auto cmd = std::make_shared<imr::CommandBuffer>(device, device.main_queue());

            VkSemaphore sem;
            vkCreateSemaphore(device.device, tmpPtr((VkSemaphoreCreateInfo) {
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            }), nullptr, &sem);
            vk.cmdPipelineBarrier2KHR(*cmd, tmpPtr((VkDependencyInfo) {
                .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .dependencyFlags = 0,
                .imageMemoryBarrierCount = 1,
                .pImageMemoryBarriers = tmpPtr((VkImageMemoryBarrier2) {
                    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                    .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
                    .srcAccessMask = VK_ACCESS_2_NONE,
                    .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                    .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                    .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                    .newLayout = VK_IMAGE_LAYOUT_GENERAL,
                    .image = image->handle(),
                    .subresourceRange = image->whole_image_subresource_range()
                }),
            }));

            vk.cmdClearColorImage(*cmd, image->handle(), VK_IMAGE_LAYOUT_GENERAL, tmpPtr((VkClearColorValue) {
                .float32 = { 0.0f, 0.0f, 0.0f, 1.0f },
            }), 1, tmpPtr(image->whole_image_subresource_range()));

            vk.cmdPipelineBarrier2KHR(*cmd, tmpPtr((VkDependencyInfo) {
                .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .dependencyFlags = 0,
                .imageMemoryBarrierCount = 1,
                .pImageMemoryBarriers = tmpPtr((VkImageMemoryBarrier2) {
                    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                    .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                    .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                    .dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                    .dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                    .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
                    .newLayout = VK_IMAGE_LAYOUT_GENERAL,
                    .image = image->handle(),
                    .subresourceRange = image->whole_image_subresource_range()
                }),
            }));

            vk.cmdBindPipeline(*cmd, VK_PIPELINE_BIND_POINT_COMPUTE, shader.pipeline());
            vk.cmdBindDescriptorSets(*cmd, VK_PIPELINE_BIND_POINT_COMPUTE, shader.layout(), 0, 1, &set, 0, nullptr);

            vk.cmdDispatch(*cmd, (image->size().width + 31) / 32, (image->size().height + 31) / 32, 1);

            vk.cmdPipelineBarrier2KHR(*cmd, tmpPtr((VkDependencyInfo) {
                .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .dependencyFlags = 0,
                .imageMemoryBarrierCount = 1,
                .pImageMemoryBarriers = tmpPtr((VkImageMemoryBarrier2) {
                    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                    .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                    .srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                    .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                    .dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT,
                    .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
                    .newLayout = VK_IMAGE_LAYOUT_GENERAL,
                    .image = image->handle(),
                    .subresourceRange = image->whole_image_subresource_range()
                }),
            }));

            cmd->submit({}, { sem });

            frame.addCleanupAction([=, &device, _= cmd]() {
                vkDestroySemaphore(device.device, sem, nullptr);
            });
            frame.presentFromImage(image->handle(), { sem }, VK_IMAGE_LAYOUT_GENERAL, std::make_optional<VkExtent2D>(image->size().width, image->size().height));
        });

        glfwPollEvents();
    }

    swapchain.drain();

    vkFreeDescriptorSets(device.device, pool, 1, &set);
    vkDestroyDescriptorPool(device.device, pool, nullptr);

    delete image;
    vkDestroyImageView(device.device, view, nullptr);

    return 0;
}
