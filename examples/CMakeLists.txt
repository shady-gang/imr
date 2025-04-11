add_subdirectory(10_intro)
add_subdirectory(11_present_image)
add_subdirectory(12_render_pipeline)

add_executable(demo main.cpp)
target_link_libraries(demo imr)

add_custom_target(demo_shader COMMAND ${GLSLANG_EXE} -V -S comp ${CMAKE_CURRENT_SOURCE_DIR}/checkerboard.glsl -o ${CMAKE_CURRENT_BINARY_DIR}/checkerboard.spv)
add_dependencies(demo demo_shader)
