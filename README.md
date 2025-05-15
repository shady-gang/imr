# imr - In Medias Res, an inconsequential Vulkan framework

You don't need to be writing this stuff yourself again and again.

IMR wraps together `vk-boostrap` and `VMA` and comes with some sane modern defaults, and a few tasteful training wheels.

See [imr.h](imr/include/imr.h) for the public API including inline documentation.

Check out the [examples](examples/) folder.

## Building

Recent CMake required.

IMR requires [shady](https://github.com/shady-gang/shady) as a dependency, to inspect SPIR-V files.
It should get automatically downloaded and installed by CMake FetchContent if the `IMR_USE_CUSTOM_shady` property is set (it's now the default).

IMR requires GLFW3, by default it does not use FetchContent to get it, and instead uses whatever version is available on your system.
You can change the `IMR_USE_CUSTOM_GLFW` property to change this.