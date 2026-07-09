#ifndef IMR_SWAPCHAIN_PRIVATE_H
#define IMR_SWAPCHAIN_PRIVATE_H

#include "imr_private.h"

namespace imr {

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
    std::vector<std::unique_ptr<Slot>> slots;
    std::optional<std::tuple<Swapchain::Slot&, VkSemaphore, VkFence>> try_acquire_slot();
    std::tuple<Swapchain::Slot&, VkSemaphore, VkFence> acquire_slot();

    size_t next_fif = 0;
    std::vector<std::unique_ptr<Frame>> frames_in_flight;
    Frame& begin_frame();
    void drain();

    void build_swapchain();
    void destroy_swapchain();
};

struct Swapchain::Slot::Impl {
    Device& device_;
    Swapchain& swapchain;
    Impl(Swapchain& s);

    uint32_t image_index;
    std::unique_ptr<Image> image;

    VkSemaphore acquire_semaphore = VK_NULL_HANDLE;
    VkSemaphore present_semaphore = VK_NULL_HANDLE;
    VkFence present_fence = VK_NULL_HANDLE;

    void queuePresent(std::vector<VkSemaphore> waits);

    std::unique_ptr<Swapchain::Frame>* frame = nullptr;

    ~Impl();
};

struct Swapchain::Frame::Impl {
    Device& device;
    Swapchain::Slot& slot;
    size_t id_;
    bool submitted = false;

    VkSemaphore frame_finished = VK_NULL_HANDLE;
    std::vector<VkFence> recycle_waits;

    Impl(Impl&) = delete;
    Impl(Device&, Slot&, size_t);
    ~Impl();

    VkSemaphore create_frame_lived_semaphore(std::string);

    std::vector<std::function<void(void)>> cleanup_queue;
};

//std::optional<std::tuple<Swapchain::Slot&, VkSemaphore>> nextSwapchainSlot(Swapchain::Impl* _impl);

}

#endif
