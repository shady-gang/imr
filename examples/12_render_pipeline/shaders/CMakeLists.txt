set(SHADERS "")

function(add_shader SHADER_LIST)
    cmake_parse_arguments(PARSE_ARGV 1 "ARG" "" "SOURCE_FILE" "ADDITIONAL_SOURCES")

    add_custom_command(
        OUTPUT ${ARG_SOURCE_FILE}.spv
        COMMAND Vulkan::glslc ${CMAKE_CURRENT_SOURCE_DIR}/${ARG_SOURCE_FILE} -o ${CMAKE_CURRENT_BINARY_DIR}/${ARG_SOURCE_FILE}.spv
        MAIN_DEPENDENCY ${ARG_SOURCE_FILE}
        DEPENDS ${ARG_ADDITIONAL_SOURCES}
        VERBATIM
        COMMAND_EXPAND_LISTS
    )

    list(APPEND ${SHADER_LIST} ${ARG_SOURCE_FILE}.spv)
    set(${SHADER_LIST} ${${SHADER_LIST}} PARENT_SCOPE)
endfunction()

add_shader(SHADERS SOURCE_FILE shader.vert ADDITIONAL_SOURCES noise.glsl)
add_shader(SHADERS SOURCE_FILE shader.frag ADDITIONAL_SOURCES noise.glsl)

message(STATUS ${SHADERS})

add_custom_target(shaders DEPENDS ${SHADERS})
