cmake_minimum_required(VERSION 3.28)
project(imr)

find_package(Vulkan REQUIRED)
#find_package(glfw3 REQUIRED)
include(FetchContent)
option(GLFW_BUILD_TESTS "" OFF)
option(GLFW_BUILD_DOCS "" OFF)
option(GLFW_INSTALL "" OFF)
option(GLFW_BUILD_EXAMPLES "" OFF)
#option(GLFW_BUILD_WAYLAND "" OFF)
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw
    GIT_TAG        3.4
)
FetchContent_MakeAvailable(glfw)

set(CMAKE_CXX_STANDARD 20)

add_subdirectory(vk-bootstrap)
add_subdirectory(VulkanMemoryAllocator)

add_subdirectory(imr)

if (PROJECT_IS_TOP_LEVEL)
    add_subdirectory(examples)
endif ()
