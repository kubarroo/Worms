file(GLOB_RECURSE WORMS_TEST_GAME_SOURCES CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/Worms/src/*.cpp"
)
list(FILTER WORMS_TEST_GAME_SOURCES EXCLUDE REGEX "/Core/Initialization/EntryPoint\\.cpp$")

add_executable(worms_tests
    TestColliderFromSprite.cpp
    TestHandleSafety.cpp
    TestCleanupSafety.cpp
    TestGameLifetime.cpp
    ${WORMS_TEST_GAME_SOURCES}
)
target_include_directories(worms_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/Worms/src"
)
target_link_libraries(worms_tests PRIVATE
    GTest::gtest_main SDL2::SDL2 box2d worms_ecs worms_imgui
    $<IF:$<TARGET_EXISTS:SDL2_image::SDL2_image>,SDL2_image::SDL2_image,SDL2_image::SDL2_image-static>
    $<IF:$<TARGET_EXISTS:SDL2_mixer::SDL2_mixer>,SDL2_mixer::SDL2_mixer,SDL2_mixer::SDL2_mixer-static>
)

if(WIN32)
    add_custom_command(TARGET worms_tests POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            $<TARGET_RUNTIME_DLLS:worms_tests>
            "$<TARGET_FILE_DIR:worms_tests>"
        COMMAND_EXPAND_LISTS
        VERBATIM
    )
endif()

include(GoogleTest)
gtest_discover_tests(worms_tests DISCOVERY_MODE PRE_TEST
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/Worms"
)
