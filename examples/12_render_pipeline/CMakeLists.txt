add_subdirectory(shaders)

find_package(shady REQUIRED)

add_executable(12_render_pipeline 12_render_pipeline.cpp libs/camera.cpp)

target_link_libraries(12_render_pipeline PRIVATE imr)
target_link_libraries(12_render_pipeline PRIVATE shady::runner shady::driver)

add_dependencies(12_render_pipeline shaders)
