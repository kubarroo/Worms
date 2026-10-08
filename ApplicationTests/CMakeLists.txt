add_executable(worms_tests
    TestColliderFromSprite.cpp
    "${PROJECT_SOURCE_DIR}/Worms/src/Core/Utils.cpp"
)
target_include_directories(worms_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/Worms/src"
)
target_link_libraries(worms_tests PRIVATE
    GTest::gtest_main SDL2::SDL2 box2d
    $<IF:$<TARGET_EXISTS:SDL2_image::SDL2_image>,SDL2_image::SDL2_image,SDL2_image::SDL2_image-static>
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
gtest_discover_tests(worms_tests DISCOVERY_MODE PRE_TEST)