#ifndef IMR_SWAPCHAIN_PRIVATE_H
#define IMR_SWAPCHAIN_PRIVATE_H

#include "imr_private.h"

namespace imr {

struct SwapchainSlot;

struct Swapchain::Impl {
    Swapchain& parent;
    Device& device;
    GLFWwindow* window = nullptr;
    Impl(Swapchain& parent, Device&, GLFWwindow*);
    ~Impl();

    VkSurfaceKHR surface;
    size_t frame_counter = 0;

    uint64_t last_present = 0;
    bool should_resize = false;

    vkb::Swapchain swapchain;
    std::vector<std::unique_ptr<SwapchainSlot>> slots;

    std::unique_ptr<Image> depth_image;

    void build_swapchain();
    void destroy_swapchain();
};

struct SwapchainSlot {
    Swapchain& swapchain;
    SwapchainSlot(Swapchain& s);
    SwapchainSlot(SwapchainSlot&) = delete;

    VkImage image;
    uint32_t image_index;

    VkSemaphore copy_done;
    VkSemaphore present_semaphore;
    VkFence wait_for_previous_present = VK_NULL_HANDLE;

    std::unique_ptr<Swapchain::Frame> frame = nullptr;

    ~SwapchainSlot();
};

struct Swapchain::Frame::Impl {
    Device& device;
    SwapchainSlot& slot;
    std::unique_ptr<Image> image;

    Impl(Impl&) = delete;
    Impl(Impl&&) = default;
    Impl& operator=(Impl&&) = default;
    Impl(Device&, SwapchainSlot&);

    std::vector<VkFence> cleanup_fences;
    std::vector<std::function<void(void)>> cleanup_queue;
};

std::optional<std::tuple<SwapchainSlot&, VkSemaphore>> nextSwapchainSlot(Swapchain::Impl* _impl);

}

#endif
