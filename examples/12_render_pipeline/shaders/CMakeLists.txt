set(SHADERS "")

get_target_property(NASL_INCLUDE shady::nasl INTERFACE_INCLUDE_DIRECTORIES)

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

function(add_shady_shader SHADER_LIST)
    cmake_parse_arguments(PARSE_ARGV 1 "ARG" "" "SOURCE_FILE;EXECUTION_MODEL" "ADDITIONAL_SOURCES")

    add_custom_command(OUTPUT ${ARG_SOURCE_FILE}.spv
        COMMAND shady::vcc
        ARGS ${CMAKE_CURRENT_SOURCE_DIR}/${ARG_SOURCE_FILE} -I${NASL_INCLUDE} -O3 --entry-point main --execution-model ${ARG_EXECUTION_MODEL} --std=c++20 -o ${CMAKE_CURRENT_BINARY_DIR}/${ARG_SOURCE_FILE}.spv
        DEPENDS ${ARG_SOURCE_FILE} ${ARG_ADDITIONAL_SOURCES})

    list(APPEND ${SHADER_LIST} ${ARG_SOURCE_FILE}.spv)
    set(${SHADER_LIST} ${${SHADER_LIST}} PARENT_SCOPE)
endfunction()

add_shader(SHADERS SOURCE_FILE shader.vert ADDITIONAL_SOURCES noise.glsl)
add_shader(SHADERS SOURCE_FILE shader.frag ADDITIONAL_SOURCES noise.glsl)

add_shady_shader(SHADERS SOURCE_FILE shader.vert.cpp EXECUTION_MODEL Vertex ADDITIONAL_SOURCES noise.h math.h)
add_shady_shader(SHADERS SOURCE_FILE shader.frag.cpp EXECUTION_MODEL Fragment ADDITIONAL_SOURCES noise.h math.h)

message(STATUS ${SHADERS})

add_custom_target(shaders DEPENDS ${SHADERS})
