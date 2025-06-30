#find_package(shady REQUIRED)
#find_package(assimp REQUIRED)

add_subdirectory(shaders)

add_executable(hpg_demo main.cpp libs/camera.cpp)

target_link_libraries(hpg_demo PRIVATE imr nasl::nasl)
target_link_libraries(hpg_demo PRIVATE shady::runner shady::driver)

add_dependencies(hpg_demo shaders)
