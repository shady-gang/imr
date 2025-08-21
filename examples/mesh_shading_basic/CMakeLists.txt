add_executable(mesh_shading mesh_shading_basic.cpp ../common/camera.cpp)
target_link_libraries(mesh_shading imr nasl::nasl)

add_custom_target(mesh_shading_mesh_spv COMMAND ${GLSLANG_EXE} -V --target-env vulkan1.3 -S mesh ${CMAKE_CURRENT_SOURCE_DIR}/meshshader.mesh -o ${CMAKE_CURRENT_BINARY_DIR}/meshshader.mesh.spv)
add_dependencies(mesh_shading mesh_shading_mesh_spv)
add_custom_target(mesh_shading_task_spv COMMAND ${GLSLANG_EXE} -V --target-env vulkan1.3 -S task ${CMAKE_CURRENT_SOURCE_DIR}/meshshader.task -o ${CMAKE_CURRENT_BINARY_DIR}/meshshader.task.spv)
add_dependencies(mesh_shading mesh_shading_task_spv)
add_custom_target(mesh_shading_frag_spv COMMAND ${GLSLANG_EXE} -V --target-env vulkan1.3 -S frag ${CMAKE_CURRENT_SOURCE_DIR}/meshshader.frag -o ${CMAKE_CURRENT_BINARY_DIR}/meshshader.frag.spv)
add_dependencies(mesh_shading mesh_shading_frag_spv)
