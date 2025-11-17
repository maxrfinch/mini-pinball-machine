cmake_minimum_required(VERSION 3.16)
cmake_policy(SET CMP0074 NEW)
cmake_policy(SET CMP0083 NEW)
cmake_policy(SET CMP0077 NEW)

project(pinball VERSION 0.1.0
        DESCRIPTION "Mini Pinball Machine"
        LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

### Require out-of-source builds
file(TO_CMAKE_PATH "${PROJECT_BINARY_DIR}/CMakeLists.txt" LOC_PATH)
if(EXISTS "${LOC_PATH}")
    message(FATAL_ERROR "You cannot build in a source directory (or any directory with a CMakeLists.txt file). Please make a build subdirectory. Feel free to remove CMakeCache.txt and CMakeFiles.")
endif()

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    if (WIN32)
    else()
        add_compile_options(-Wall -Wno-ignored-attributes)
    endif()
    set_property(GLOBAL PROPERTY USE_FOLDERS ON)
endif()

#---------------- SOURCE FILES ----------------
set(SOURCES
    src/constants.c 
    src/gameStruct.c 
    src/inputManagerMac.c 
    src/main.c 
    src/physics.c 
    src/physicsDebugDraw.c 
    src/scores.c 
    src/soundManager.c 
    src/sqlite3.c 
)

# Note: physics_old.c, physicsDebugDraw_old.c are reference implementations
# and are NOT included in the build. They exist for comparing behavior.

set(HEADERS_PRIVATE
    src/constants.h 
    src/gameStruct.h
    src/inputManager.h
    src/physics.h
    src/physicsDebugDraw.h
    src/scores.h
    src/soundManager.h
    src/sqlite3.h
)

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS_PRIVATE})
set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_NAME "pinball"
    LINKER_LANGUAGE C
    MACOSX_RPATH TRUE
)

target_include_directories(${PROJECT_NAME} PUBLIC
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_BINARY_DIR}
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/resources/assets
            $<TARGET_FILE_DIR:${PROJECT_NAME}>/Resources)

#--------------- HOMEBREW LIBRARIES --------------------

# Prefer Homebrew on Apple Silicon (M1/M2) but also fall back to /usr/local for Intel/macOS
if(APPLE)
    list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew" "/usr/local")
endif()

# raylib from Homebrew
find_path(RAYLIB_INCLUDE_DIR
    NAMES raylib.h
    PATHS
        /opt/homebrew/opt/raylib/include
        /opt/homebrew/include
        /usr/local/include
)

find_library(RAYLIB_LIB
    NAMES raylib
    PATHS
        /opt/homebrew/opt/raylib/lib
        /opt/homebrew/lib
        /usr/local/lib
)

message(STATUS "RAYLIB_INCLUDE_DIR = ${RAYLIB_INCLUDE_DIR}")
message(STATUS "RAYLIB_LIB         = ${RAYLIB_LIB}")

if(NOT RAYLIB_INCLUDE_DIR OR NOT RAYLIB_LIB)
    message(FATAL_ERROR "raylib not found. Install with 'brew install raylib' or adjust paths in CMakeLists.txt")
endif()

# Box2D from Homebrew (3.x C API)
find_path(BOX2D_INCLUDE_DIR
    NAMES box2d/box2d.h
    PATHS
        /opt/homebrew/include
        /usr/local/include
)

find_library(BOX2D_LIB
    NAMES box2d
    PATHS
        /opt/homebrew/lib
        /usr/local/lib
)

if(NOT BOX2D_INCLUDE_DIR OR NOT BOX2D_LIB)
    message(FATAL_ERROR "Box2D physics library not found. Install with 'brew install box2d' and adjust paths if needed.")
endif()

message(STATUS "BOX2D_INCLUDE_DIR = ${BOX2D_INCLUDE_DIR}")
message(STATUS "BOX2D_LIB         = ${BOX2D_LIB}")

target_include_directories(${PROJECT_NAME} PRIVATE
    ${RAYLIB_INCLUDE_DIR}
    ${BOX2D_INCLUDE_DIR}
)

# Link against Homebrew raylib and box2d
target_link_libraries(${PROJECT_NAME} PRIVATE
    ${RAYLIB_LIB}
    ${BOX2D_LIB}
)

#--------------- RPATH FOR HOMEBREW DYLIBS ON MAC --------------------

if(APPLE)
    # Add Homebrew lib dirs to rpath so dylibs are found at runtime
    set(HOMEBREW_RPATHS "")

    # Try to query Homebrew prefixes at configure time
    find_program(HOMEBREW_EXECUTABLE brew)
    if(HOMEBREW_EXECUTABLE)
        # Box2D
        execute_process(
            COMMAND "${HOMEBREW_EXECUTABLE}" --prefix box2d
            OUTPUT_VARIABLE BOX2D_PREFIX
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
        if(BOX2D_PREFIX)
            list(APPEND HOMEBREW_RPATHS "${BOX2D_PREFIX}/lib")
        endif()

        # raylib
        execute_process(
            COMMAND "${HOMEBREW_EXECUTABLE}" --prefix raylib
            OUTPUT_VARIABLE RAYLIB_PREFIX
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
        if(RAYLIB_PREFIX)
            list(APPEND HOMEBREW_RPATHS "${RAYLIB_PREFIX}/lib")
        endif()
    endif()

    # Generic fallbacks in case execute_process failed or Homebrew is somewhere else
    list(APPEND HOMEBREW_RPATHS "/opt/homebrew/lib" "/usr/local/lib")

    # Apply rpaths to the target so dyld can find the dylibs at runtime
    set_target_properties(${PROJECT_NAME} PROPERTIES
        BUILD_RPATH "${HOMEBREW_RPATHS}"
        INSTALL_RPATH "${HOMEBREW_RPATHS}"
    )
endif()

#--------------- PLATFORM FLAGS --------------------

if (WIN32)
    add_definitions(-DPLATFORM_WIN32)
elseif(APPLE)
    add_definitions(-DPLATFORM_APPLE)
    # raylib’s required macOS frameworks
    target_link_libraries(${PROJECT_NAME} PRIVATE
        "-framework Cocoa"
        "-framework OpenGL"
        "-framework IOKit"
        "-framework CoreVideo"
    )
elseif(UNIX)
    add_definitions(-DPLATFORM_LINUX)
    target_link_libraries(${PROJECT_NAME} PRIVATE m pthread dl)
else()
    message(FATAL_ERROR "Unknown platform!")
endif()

source_group(TREE "${PROJECT_SOURCE_DIR}"
             PREFIX "Header Files"
             FILES ${HEADERS_PRIVATE})