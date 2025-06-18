add_executable(rap_god rap_god.cpp ../examples/common/camera.cpp)
target_link_libraries(rap_god imr nasl::nasl)

add_custom_target(rap_god_vert_spv COMMAND ${GLSLANG_EXE} -V -S vert ${CMAKE_CURRENT_SOURCE_DIR}/rap_god.vert -o ${CMAKE_CURRENT_BINARY_DIR}/rap_god.vert.spv)
add_dependencies(rap_god rap_god_vert_spv)
add_custom_target(rap_god_frag_spv COMMAND ${GLSLANG_EXE} -V -S frag ${CMAKE_CURRENT_SOURCE_DIR}/rap_god.frag -o ${CMAKE_CURRENT_BINARY_DIR}/rap_god.frag.spv)
add_dependencies(rap_god rap_god_frag_spv)
