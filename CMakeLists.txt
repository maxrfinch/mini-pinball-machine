cmake_minimum_required(VERSION 3.15)

project(pinball C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# Source files
# ---------------------------------------------------------------------------

# Explicit list is safer than a glob; keep this in sync with src/ directory.
set(SRC_FILES
    src/constants.c
    src/gameStruct.c
    src/game.c
    src/inputManagerMac.c
    src/main.c
    src/menu.c
    src/physics.c
    src/physicsDebugDraw.c
    src/powerups.c
    src/render.c
    src/resources.c
    src/scores.c
    src/soundManager.c
    src/sqlite3.c
    src/ui.c
    src/util.c
    src/water.c
)

add_executable(${PROJECT_NAME} ${SRC_FILES})

# ---------------------------------------------------------------------------
# Dependencies: raylib + Box2D
# ---------------------------------------------------------------------------

# Allow caller to override via -DRAYLIB_INCLUDE_DIR / -DRAYLIB_LIB
if (NOT DEFINED RAYLIB_INCLUDE_DIR OR NOT DEFINED RAYLIB_LIB)
    find_path(RAYLIB_INCLUDE_DIR raylib.h
        PATHS /usr/local/include /usr/include
        PATH_SUFFIXES raylib
    )
    find_library(RAYLIB_LIB NAMES raylib
        PATHS /usr/local/lib /usr/lib
    )
endif()

if (NOT RAYLIB_INCLUDE_DIR OR NOT RAYLIB_LIB)
    message(FATAL_ERROR
        "raylib not found.\n"
        "On macOS (Homebrew):    brew install raylib\n"
        "On Debian/Raspberry Pi: build/install raylib from source, then rerun cmake with:\n"
        "  -DRAYLIB_INCLUDE_DIR=/usr/local/include -DRAYLIB_LIB=/usr/local/lib/libraylib.a"
    )
endif()

# Allow caller to override via -DBOX2D_INCLUDE_DIR / -DBOX2D_LIB
if (NOT DEFINED BOX2D_INCLUDE_DIR OR NOT DEFINED BOX2D_LIB)
    find_path(BOX2D_INCLUDE_DIR box2d/box2d.h
        PATHS /usr/local/include /usr/include
    )
    find_library(BOX2D_LIB NAMES box2d
        PATHS /usr/local/lib /usr/lib
    )
endif()

if (NOT BOX2D_INCLUDE_DIR OR NOT BOX2D_LIB)
    message(FATAL_ERROR
        "Box2D not found.\n"
        "Build/install Box2D 3.x and rerun cmake with:\n"
        "  -DBOX2D_INCLUDE_DIR=/usr/local/include -DBOX2D_LIB=/usr/local/lib/libbox2d.a"
    )
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
    ${RAYLIB_INCLUDE_DIR}
    ${BOX2D_INCLUDE_DIR}
    src
)

# ---------------------------------------------------------------------------
# Platform flags & linking
# ---------------------------------------------------------------------------

if (WIN32)
    add_definitions(-DPLATFORM_WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        ${RAYLIB_LIB}
        ${BOX2D_LIB}
        winmm
    )

elseif(APPLE)
    add_definitions(-DPLATFORM_APPLE)

    # raylib’s required macOS frameworks
    target_link_libraries(${PROJECT_NAME} PRIVATE
        ${RAYLIB_LIB}
        ${BOX2D_LIB}
        "-framework Cocoa"
        "-framework OpenGL"
        "-framework IOKit"
        "-framework CoreVideo"
    )

elseif(UNIX) # Linux / Raspberry Pi
    add_definitions(-DPLATFORM_LINUX)

    # For Raspberry Pi 5 cabinet build we are using raylib built with PLATFORM=DRM
    # and OpenGL ES 2.0. Link the full DRM/GLES stack explicitly.
    target_link_libraries(${PROJECT_NAME} PRIVATE
        ${RAYLIB_LIB}
        ${BOX2D_LIB}
        m
        pthread
        dl
        EGL
        GLESv2
        drm
        gbm
        xkbcommon
    )

else()
    message(FATAL_ERROR "Unknown platform!")
endif()