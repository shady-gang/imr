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

template<typename T>
/// A lot of Vulkan calls require pointers to structures, but often the data we want to provide is just some expression.
/// This utility method is used to leak the address of a temporary, which in C++ is specified to live for the duration of the statement
inline T* tmpPtr(T&& t) { return &t; }

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

/// Deals with the common use-cases for images, allocating memory for you and tracking properties.
/// Does not track image layouts for you, much of the framework assumes VK_IMAGE_LAYOUT_GENERAL
struct Image {
    VkImage handle() const;

    VkImageType type() const;
    VkExtent3D size() const;
    VkFormat format() const;

    Image(Device&, VkImageType dim, VkExtent3D size, VkFormat format, VkImageUsageFlagBits usage);
    Image(Image&) = delete;
    Image(Image&&);
    ~Image();

    VkImageSubresourceRange whole_image_subresource_range() const;

    struct Impl;
    Image(Impl&&);
private:
    std::unique_ptr<Impl> _impl;
};

/// Helper class that allocates, populates and binds descriptor sets for us
/// Since it owns the descriptor sets internally, it must live as they are in use
/// Therefore, it should not be stack-allocated inside e.g. the beginFrame lambda !
struct DescriptorBindHelper {
    class Impl;

    explicit DescriptorBindHelper(std::unique_ptr<Impl>&&);
    DescriptorBindHelper(DescriptorBindHelper&) = delete;
    ~DescriptorBindHelper();

    void set_storage_image(uint32_t set, uint32_t binding, Image& image, std::optional<VkImageSubresourceRange> = std::nullopt, std::optional<VkImageViewType> = std::nullopt);
    void commit(VkCommandBuffer);

    std::unique_ptr<Impl> _impl;
};

struct ComputeShader {
    ComputeShader(Device&, std::string&& name, std::string&& entrypoint_name = "main");
    ComputeShader(ComputeShader&) = delete;
    ComputeShader(ComputeShader&&);
    ~ComputeShader();

    VkPipeline pipeline() const;
    VkPipelineLayout layout() const;
    VkDescriptorSetLayout set_layout(unsigned) const;

    DescriptorBindHelper* create_bind_helper();

    struct Impl;
    std::unique_ptr<Impl> _impl;
};

struct Swapchain {
    Swapchain(Device&, GLFWwindow* window);
    ~Swapchain();

    Device& device() const;
    VkFormat format() const;

    /// Approximate FPS cap, avoids melting your GPU on a trivial scene
    int maxFps = 999;

    struct Frame {
        void presentFromBuffer(VkBuffer buffer, VkFence signal_when_reusable, std::optional<VkSemaphore> sem);
        void presentFromImage(VkImage image, VkFence signal_when_reusable, std::optional<VkSemaphore> sem, VkImageLayout src_layout = VK_IMAGE_LAYOUT_GENERAL, std::optional<VkExtent2D> image_size = std::nullopt);

        size_t id;
        Image& image() const;
        VkSemaphore swapchain_image_available;
        VkSemaphore signal_when_ready;
        void queuePresent();

        void addCleanupFence(VkFence fence);
        void addCleanupAction(std::function<void(void)>&& fn);

        class Impl;
        std::unique_ptr<Impl> _impl;

        Frame(Impl&&);
        Frame(Frame&) = delete;
        ~Frame();
    };

    /// API to obtain a swapchain image, do some rendering with it and then ultimately queuePresent it.
    /// You have to consume the `swapchain_image_available` semaphore
    /// You have to signal the `signal_when_ready` semaphore
    /// You have to call queuePresent() when you're done
    void beginFrame(std::function<void(Swapchain::Frame&)>&& fn);

    struct SimplifiedRenderContext {
        virtual Image& image() const = 0;
        virtual VkCommandBuffer cmdbuf() const = 0;

        virtual void addCleanupAction(std::function<void(void)>&& fn) = 0;
    };

    /// Simplified API to draw a frame, deals with cmdbuf allocation, recording and submission, as well as layout transitions in and out of VK_IMAGE_LAYOUT_GENERAL for the swapchain image
    void renderFrameSimplified(std::function<void(SimplifiedRenderContext&)>&& fn);

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
