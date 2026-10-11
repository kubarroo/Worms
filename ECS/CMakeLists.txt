add_library(worms_ecs INTERFACE)
target_include_directories(worms_ecs INTERFACE "${PROJECT_SOURCE_DIR}")
target_link_libraries(worms_ecs INTERFACE EnTT::EnTT)
