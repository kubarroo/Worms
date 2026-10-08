# Worms

A local multiplayer game inspired by Worms Armageddon, originally
developed for a university computer graphics course.

## Features

- Local multiplayer with teams of worms
- Box2D physics and destructible terrain
- Health and turn management
- Bazooka and grenade weapons
- Sound effects and music
- Dear ImGui debug tools
- Custom ECS, planned for replacement with EnTT

## Requirements

- Windows
- CMake 3.24 or newer
- Ninja
- LLVM/Clang with C++20 support
- Windows SDK and compatible C/C++ runtime development libraries
- Git and Git LFS

Dependencies are provided through Git submodules:
SDL2, SDL2_image, SDL2_mixer, Dear ImGui, and Box2D 2.4.1.

Game assets and PDF documentation are stored using Git LFS.

## Clone

Install Git LFS before cloning:

    git lfs install
    git clone --recurse-submodules <repository-url>
    cd Worms
    git lfs pull

For an existing checkout:

    git submodule update --init --recursive
    git lfs pull

## Build

Run these commands from the repository root.

Debug:

    cmake --preset debug
    cmake --build --preset debug

Release:

    cmake --preset release
    cmake --build --preset release

CMake must run in an environment where the compiler, Windows SDK,
and runtime development libraries are available. Select the Clang
toolchain locally; do not commit machine-specific absolute paths.

## Run

Debug:

    .\build\debug\Worms\worms.exe

Release:

    .\build\release\Worms\worms.exe

The build copies assets and required dependency DLLs beside the
executable. Keep these files together when moving the application.

The application sets its working directory to the executable
directory at startup. Runtime logs are written to logs.txt there.

## Development

Configure the Debug preset to generate:

    build/debug/compile_commands.json

Point clangd to build/debug. Formatting and static analysis settings
are defined in .clang-format and .clang-tidy.

For VS Code debugging, use .vscode/launch.json and build the Debug
configuration before pressing F5.

## Tests

ApplicationTests contains terrain contour tests. These tests are
not yet integrated into the CMake build.

## Documentation

See docs/Worms_documentation.pdf for the original project documentation.
It may not reflect subsequent architecture changes.

## Planned Changes

- Migrate SDL2 to SDL3
- Replace the custom ECS with EnTT
- Improve resource ownership and object lifetimes
- Refactor scene management and gameplay systems

## License

See LICENSE.txt. Third-party libraries retain their own licenses.