cmake_minimum_required(VERSION 3.20)
project(My2DGame LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.2.10
)

FetchContent_MakeAvailable(SDL3)

add_executable(My2DGame
    src/main.cpp
    src/ai/AiDebugLog.cpp
    src/ai/AiRuntimeManager.cpp
    src/ai/LocalLLMClient.cpp
    src/ai/NpcChatSystem.cpp
    src/ai/NpcCommandAI.cpp
    src/core/App.cpp
    src/core/Camera2D.cpp
    src/core/HudRenderer.cpp
    src/core/PixelFont.cpp
    src/game/Actor.h
    src/game/Animal.cpp
    src/game/HostileAI.cpp
    src/game/NpcAI.cpp
    src/game/Pawn.h
    src/game/Pet.cpp
    src/game/Player.cpp
    src/game/TileMap.cpp
)

target_include_directories(My2DGame PRIVATE src)

target_link_libraries(My2DGame PRIVATE SDL3::SDL3)

if(WIN32)
    # Ensure SDL runtime DLL is available next to the game executable.
    add_custom_command(TARGET My2DGame POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3>
            $<TARGET_FILE_DIR:My2DGame>
    )
endif()
