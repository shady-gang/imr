add_executable(13_compute_triangle 13_compute_triangle.cpp)
target_link_libraries(13_compute_triangle imr)

add_custom_target(13_compute_triangle_spv COMMAND ${GLSLANG_EXE} -V -S comp ${CMAKE_CURRENT_SOURCE_DIR}/13_compute_triangle.glsl -o ${CMAKE_CURRENT_BINARY_DIR}/13_compute_triangle.spv )
add_dependencies(13_compute_triangle 13_compute_triangle_spv)
