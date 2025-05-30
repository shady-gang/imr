#if (TARGET shady::vcc)
#message("Vcc available")
    add_subdirectory(shaders)

    add_subdirectory(libs)
    message(STATUS ${LIBS})
    add_executable(20_render_pipeline 20_render_pipeline.cpp ${LIBS})

    find_package(assimp REQUIRED)

    target_link_libraries(20_render_pipeline PRIVATE imr)
    target_link_libraries(20_render_pipeline PRIVATE assimp)
    target_link_libraries(20_render_pipeline PRIVATE shady::runner shady::driver)

    find_path(SPACENAVD spnav.h)
    if (SPACENAVD)
        #Find releases of libspnav here: https://github.com/FreeSpacenav/libspnav
        #Compile such a release version into libs/libspacenav to add support for 6-dof input devices.
        add_compile_definitions(SPACENAVD)
        target_link_libraries(20_render_pipeline PRIVATE X11)
        target_link_libraries(20_render_pipeline PRIVATE spnav)
    endif()

    add_dependencies(20_render_pipeline shaders)
#endif()
