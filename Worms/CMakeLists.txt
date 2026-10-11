file(GLOB_RECURSE WORMS_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_executable(worms WIN32 ${WORMS_SOURCES})
target_include_directories(worms PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_link_libraries(worms PRIVATE
    EnTT::EnTT
    worms_imgui box2d SDL3::SDL3
    $<IF:$<TARGET_EXISTS:SDL3_image::SDL3_image>,SDL3_image::SDL3_image,SDL3_image::SDL3_image-static>
    $<IF:$<TARGET_EXISTS:SDL3_mixer::SDL3_mixer>,SDL3_mixer::SDL3_mixer,SDL3_mixer::SDL3_mixer-static>
)

file(GLOB WORMS_ASSETS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.png"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.wav"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.ogg"
)
add_custom_target(worms_assets
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        ${WORMS_ASSETS} "$<TARGET_FILE_DIR:worms>"
    DEPENDS ${WORMS_ASSETS}
    VERBATIM
)
add_dependencies(worms worms_assets)
set_target_properties(worms PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:worms>"
)

if(WIN32)
    add_custom_command(TARGET worms POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            $<TARGET_RUNTIME_DLLS:worms>
            "$<TARGET_FILE_DIR:worms>"
        COMMAND_EXPAND_LISTS
        VERBATIM
    )
endif()
