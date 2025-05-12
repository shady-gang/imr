#ifndef IMR_H
#define IMR_H

#include "vulkan/vulkan_core.h"
#include "GLFW/glfw3.h"
#include "VkBootstrap.h"

#include <functional>
#include <memory>
#include <optional>

#include <cstdio>

#define CHECK_VK(op, else) if (op != VK_SUCCESS) { fprintf(stderr, "Check failed at %s\n", #op); else; }

inline auto tmp(auto&& t) { return &t; }

namespace imr {
    struct Context {
        Context(std::function<void(vkb::InstanceBuilder&)>&& instance_custom = [](auto&) {});
        Context(Context&) = delete;
        ~Context();

        vkb::Instance instance;
        vkb::InstanceDispatchTable dispatch;

        std::vector<vkb::PhysicalDevice> available_devices(std::function<void(vkb::PhysicalDeviceSelector&)>&& device_custom = [](auto&) {});
    };

    struct Device {
        Device(Context&, std::function<void(vkb::PhysicalDeviceSelector&)>&& device_custom = [](auto&) {});
        Device(Context&, vkb::PhysicalDevice);
        Device(Device&) = delete;
        ~Device();

        Context& context;

        vkb::PhysicalDevice physical_device;
        vkb::Device device;

        VkQueue main_queue;
        uint32_t main_queue_idx;

        VkCommandPool pool;

        vkb::DispatchTable dispatch;

        class Impl;
        std::unique_ptr<Impl> _impl;
    };

    struct Buffer {
        Buffer(Device&, size_t size, VkBufferUsageFlags usage, VkMemoryPropertyFlags memory_property = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        Buffer(Buffer&) = delete;
        ~Buffer();

        size_t const size;
        VkBuffer handle;
        /// 64-bit virtual address of the buffer on the GPU
        VkDeviceAddress device_address;
        /// Managed by the allocator, required for mapping the buffer
        VkDeviceMemory memory;
        size_t memory_offset;

        struct Impl;
        std::unique_ptr<Impl> _impl;
    };

    struct Image {
        VkImageType dim;
        VkExtent3D size;
        VkFormat format;
        VkImageUsageFlagBits usage;

        VkImage handle;

        Image(Device&, VkImageType dim, VkExtent3D size, VkFormat format, VkImageUsageFlagBits usage);
        Image(Image&) = delete;
        ~Image();

        struct Impl;
        std::unique_ptr<Impl> _impl;
    };

    struct ImageState {
        Image& image;
        VkImageLayout layout;
    };

    struct Swapchain {
        Swapchain(Device&, GLFWwindow* window);
        ~Swapchain();

        VkFormat format() const;
        int maxFps = 999;

        struct Frame {
            void presentFromBuffer(VkBuffer buffer, VkFence signal_when_reusable, std::optional<VkSemaphore> sem);
            void presentFromImage(VkImage image, VkFence signal_when_reusable, std::optional<VkSemaphore> sem, VkImageLayout src_layout = VK_IMAGE_LAYOUT_GENERAL, std::optional<VkExtent2D> image_size = std::nullopt);

            size_t id;
            size_t width, height;
            VkImage swapchain_image;
            VkImage depth_image;
            VkSemaphore swapchain_image_available;
            void present(std::optional<VkSemaphore> sem);

            void add_to_delete_queue(std::optional<VkFence> fence, std::function<void(void)>&& fn);

            class Impl;
            std::unique_ptr<Impl> _impl;

            Frame(Impl&&);
            Frame(Frame&) = delete;
            ~Frame();
        };

        void beginFrame(std::function<void(Swapchain::Frame&)>&& fn);

        void resize();

        /// Waits until all the in-flight frames are done and runs their cleanup jobs
        void drain();

        class Impl;
        std::unique_ptr<Impl> _impl;
    };

    struct FpsCounter {
        FpsCounter();
        FpsCounter(FpsCounter&) = delete;
        ~FpsCounter();

        void tick();
        int average_fps();
        float average_frametime();
        void updateGlfwWindowTitle(GLFWwindow*);

        class Impl;
        std::unique_ptr<Impl> _impl;
    };
}

#endif
