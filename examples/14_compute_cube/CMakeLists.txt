add_executable(14_compute_cube 14_compute_cube.cpp)
target_link_libraries(14_compute_cube imr)

add_custom_target(14_compute_cube_spv COMMAND ${GLSLANG_EXE} -V -S comp ${CMAKE_CURRENT_SOURCE_DIR}/14_compute_cube.glsl -o ${CMAKE_CURRENT_BINARY_DIR}/14_compute_cube.spv )
add_dependencies(14_compute_cube 14_compute_cube_spv)
