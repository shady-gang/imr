add_library(imr
        src/context.cpp
        src/device.cpp
        src/swapchain.cpp
        src/command.cpp
        src/buffer.cpp
        src/image.cpp
        src/fps_counter.cpp
        src/shader.cpp
        src/graphics_pipeline.cpp
        src/frame.cpp
        src/present_helpers.cpp
        src/render_simplified.cpp
        src/descriptor_bind_helper.cpp
        src/render_targets_helper.cpp
        src/execute_commands.cpp
        src/vma.cpp
        src/util.c
)
target_include_directories(imr PUBLIC "include")
target_link_libraries(imr PUBLIC glfw Vulkan::Vulkan vk-bootstrap::vk-bootstrap GPUOpen::VulkanMemoryAllocator shady::driver)
target_link_libraries(imr PRIVATE rustex)

find_program(GLSLANG_EXE glslang glslangValidator REQUIRED)