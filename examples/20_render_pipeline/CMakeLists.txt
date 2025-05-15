if (TARGET shady::vcc)
    message("Vcc available")
    add_subdirectory(shaders)

    add_executable(20_render_pipeline 20_render_pipeline.cpp libs/camera.cpp)

    target_link_libraries(20_render_pipeline PRIVATE imr)
    target_link_libraries(20_render_pipeline PRIVATE shady::runner shady::driver)

    add_dependencies(20_render_pipeline shaders)
endif()