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
    src/physicsDebugDraw.c 
    src/scores.c 
    src/soundManager.c 
    src/sqlite3.c 
)

set(HEADERS_PRIVATE
    src/constants.h 
    src/gameStruct.h
    src/inputManager.h
    src/physicsDebugDraw.h
    src/scores.h
    src/soundManager.h
    src/sqlite3.h
)

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS_PRIVATE})
set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_NAME "pinball"
    LINKER_LANGUAGE C
    SKIP_BUILD_RPATH TRUE
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

#--------------- LOCAL LIBRARIES --------------------

set(CHIPMUNK_ROOT ${PROJECT_SOURCE_DIR}/lib/chipmunk_mac)
set(RAYLIB_ROOT   ${PROJECT_SOURCE_DIR}/lib/raylib_mac)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CHIPMUNK_ROOT}/include
    ${RAYLIB_ROOT}/include
)

target_link_directories(${PROJECT_NAME} PRIVATE
    ${CHIPMUNK_ROOT}/lib
    ${RAYLIB_ROOT}/lib
)

# link against the actual .a/.dylib names in those dirs
target_link_libraries(${PROJECT_NAME} PRIVATE raylib chipmunk)

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

source_group(TREE "${PROJECT_SOURCE_DIR}/include"
             PREFIX "Header Files"
             FILES ${HEADERS_PRIVATE})