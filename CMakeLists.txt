cmake_minimum_required(VERSION 3.28)
project(imr)

find_package(Vulkan REQUIRED)
option(IMR_USE_CUSTOM_GLFW "" OFF)
if (IMR_USE_CUSTOM_GLFW)
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
else ()
    find_package(glfw3 REQUIRED)
endif ()

option(IMR_USE_CUSTOM_shady "" ON)
if (IMR_USE_CUSTOM_shady)
    message("Grabbing shady using FetchContent")
    include(FetchContent)
    FetchContent_Declare(
        shady
        GIT_REPOSITORY https://github.com/shady-gang/shady
        GIT_TAG        imr
    )
    FetchContent_MakeAvailable(shady)
else ()
    find_package(shady REQUIRED)
    find_package(nasl REQUIRED)
endif ()

set(CMAKE_CXX_STANDARD 20)

add_subdirectory(vk-bootstrap)
add_subdirectory(VulkanMemoryAllocator)

add_subdirectory(imr)

if (PROJECT_IS_TOP_LEVEL)
    add_subdirectory(examples)
endif ()
