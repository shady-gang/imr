add_executable(15_compute_cubes 15_compute_cubes.cpp ../common/camera.cpp)
target_link_libraries(15_compute_cubes imr nasl::nasl)

add_custom_target(15_compute_cubes_spv COMMAND ${GLSLANG_EXE} -V -S comp ${CMAKE_CURRENT_SOURCE_DIR}/15_compute_cubes.glsl -o ${CMAKE_CURRENT_BINARY_DIR}/15_compute_cubes.spv --target-env vulkan1.2)
add_dependencies(15_compute_cubes 15_compute_cubes_spv)
