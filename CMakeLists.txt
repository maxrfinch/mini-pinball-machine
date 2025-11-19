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
    target_link_libraries(${PROJECT_NAME} PRIVATE
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