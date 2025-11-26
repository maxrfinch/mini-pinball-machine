cmake_minimum_required(VERSION 3.15)

project(pinball C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# Source files
# ---------------------------------------------------------------------------

# Explicit list is safer than a glob; keep this in sync with src/ directory.
set(SRC_FILES
    src/constants.c
    src/gameStruct.c
    src/game.c
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
    src/camera_wrapper.cpp
)

# On Raspberry Pi, add the C++ libcamera wrapper
if(UNIX AND NOT APPLE)
    list(APPEND SRC_FILES src/camera_libcamera.cpp)
endif()

# Platform-specific input manager implementation
if(APPLE)
    list(APPEND SRC_FILES src/inputManagerMac.c)
elseif(UNIX)
    list(APPEND SRC_FILES src/inputManagerPi.c)
else()
    message(FATAL_ERROR "No input manager defined for this platform")
endif()

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
        PATHS
            /usr/local/include
            /usr/include
            /usr/local/opt/box2d/include   # Homebrew (Intel macOS)
            /opt/homebrew/include          # Homebrew (Apple Silicon)
    )
    find_library(BOX2D_LIB NAMES box2d
        PATHS
            /usr/local/lib
            /usr/lib
            /usr/local/opt/box2d/lib       # Homebrew (Intel macOS)
            /opt/homebrew/lib              # Homebrew (Apple Silicon)
    )
endif()

if (NOT BOX2D_INCLUDE_DIR OR NOT BOX2D_LIB)
    message(FATAL_ERROR
        "Box2D not found.\n"
        "Build/install Box2D 3.x and rerun cmake with, for example:\n"
        "  -DBOX2D_INCLUDE_DIR=/usr/local/opt/box2d/include -DBOX2D_LIB=/usr/local/opt/box2d/lib/libbox2d.dylib"
    )
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
    ${RAYLIB_INCLUDE_DIR}
    ${BOX2D_INCLUDE_DIR}
    src
)

# ---------------------------------------------------------------------------
# Resources: copy data files next to the executable
# ---------------------------------------------------------------------------
set(GAME_RESOURCES_DIR "${CMAKE_SOURCE_DIR}/Resources")
if (EXISTS "${GAME_RESOURCES_DIR}")
    # First, copy the Resources tree as-is
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${GAME_RESOURCES_DIR}"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/Resources"
        COMMENT "Copying game Resources to build output"
    )
    # Then, if there is a Resources/assets folder, flatten its contents so that
    # paths like Resources/Audio/... and Resources/Textures/... resolve correctly.
    if (EXISTS "${GAME_RESOURCES_DIR}/assets")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${GAME_RESOURCES_DIR}/assets"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/Resources"
            COMMENT "Flattening Resources/assets into Resources for runtime paths"
        )
    endif()
endif()

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

elseif(UNIX) # Linux / Raspberry Pi with desktop/GLFW
    add_definitions(-DPLATFORM_LINUX)

    # Detect if we're on Raspberry Pi
    set(IS_RPI FALSE)
    if(EXISTS "/proc/device-tree/model")
        file(READ "/proc/device-tree/model" PI_MODEL)
        if(PI_MODEL MATCHES "Raspberry Pi")
            add_definitions(-DPLATFORM_RPI)
            set(IS_RPI TRUE)
            message(STATUS "Detected Raspberry Pi - enabling Pi-specific features")
        endif()
    endif()

    # Standard desktop build: raylib uses GLFW/X11 or Wayland underneath.
    set(LINUX_LIBS
        ${RAYLIB_LIB}
        ${BOX2D_LIB}
        m
        pthread
        dl
    )
    
    # On Raspberry Pi, add libcamera and libjpeg
    if(IS_RPI)
        # Find libcamera
        find_library(LIBCAMERA_LIB NAMES camera PATHS /usr/lib /usr/local/lib)
        find_library(LIBCAMERA_BASE_LIB NAMES camera-base PATHS /usr/lib /usr/local/lib)
        find_library(LIBJPEG_LIB NAMES jpeg PATHS /usr/lib /usr/local/lib)
        
        if(LIBCAMERA_LIB AND LIBCAMERA_BASE_LIB)
            list(APPEND LINUX_LIBS ${LIBCAMERA_LIB} ${LIBCAMERA_BASE_LIB})
            message(STATUS "Found libcamera: ${LIBCAMERA_LIB}")
        else()
            message(WARNING "libcamera not found - camera features will be disabled")
        endif()
        
        if(LIBJPEG_LIB)
            list(APPEND LINUX_LIBS ${LIBJPEG_LIB})
        endif()

        # Add libcamera headers from local checkout (../libcamera/libcamera/libcamera.h)
        target_include_directories(${PROJECT_NAME} PRIVATE
            ${CMAKE_SOURCE_DIR}/../libcamera
        )
    endif()
    
    target_link_libraries(${PROJECT_NAME} PRIVATE ${LINUX_LIBS})

else()
    message(FATAL_ERROR "Unknown platform!")
endif()