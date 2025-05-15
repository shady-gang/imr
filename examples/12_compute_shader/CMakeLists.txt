add_executable(12_compute_shader 12_compute_shader.cpp)
target_link_libraries(12_compute_shader imr)

add_custom_target(12_compute_shader_spv COMMAND ${GLSLANG_EXE} -V -S comp ${CMAKE_CURRENT_SOURCE_DIR}/12_compute_shader.glsl -o ${CMAKE_CURRENT_BINARY_DIR}/12_compute_shader.spv )
add_dependencies(12_compute_shader 12_compute_shader_spv)
