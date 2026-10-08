add_library(worms_ecs STATIC
    World.cpp
    EntityManager.cpp
    ComponentManager.cpp
    SystemManager.cpp
)

target_include_directories(worms_ecs PUBLIC
    "${PROJECT_SOURCE_DIR}"
)

target_link_libraries(worms_ecs PUBLIC SDL2::SDL2)