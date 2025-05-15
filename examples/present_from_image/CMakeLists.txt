add_executable(present_from_image present_from_image.cpp)
target_link_libraries(present_from_image imr)

add_custom_target(present_from_image_spv COMMAND ${GLSLANG_EXE} -V -S comp ${CMAKE_CURRENT_SOURCE_DIR}/present_from_image.glsl -o ${CMAKE_CURRENT_BINARY_DIR}/present_from_image.spv)
add_dependencies(present_from_image present_from_image_spv)
