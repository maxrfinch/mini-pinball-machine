#include "camera.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#if defined(PLATFORM_RPI)
#include "camera_libcamera.hpp"

// Internal C++ instance (opaque to C code)
struct CameraSystemInternal {
    LibcameraWrapper *wrapper;
    unsigned char *frame_buffer;
    size_t frame_size;
};

#endif

// C API Implementation

#if defined(PLATFORM_RPI)
// Storage for internal camera instance
static CameraSystemInternal *g_camera_internal = nullptr;
#endif

int Camera_Init(CameraSystem *camera) {
    if (camera == NULL) return 0;
    
    camera->initialized = 0;
    camera->preview_active = 0;
    camera->preview_tex = (Texture2D){0};
    camera->preview_width = 150;
    camera->preview_height = 150;
    camera->zoom_factor = 1.0f;
    
#if defined(PLATFORM_RPI)
    // Clean up any existing instance
    if (g_camera_internal != nullptr) {
        delete g_camera_internal->wrapper;
        delete g_camera_internal;
        g_camera_internal = nullptr;
    }
    
    g_camera_internal = new CameraSystemInternal();
    g_camera_internal->wrapper = new LibcameraWrapper();
    g_camera_internal->frame_buffer = nullptr;
    g_camera_internal->frame_size = 0;
    
    if (!g_camera_internal->wrapper->init()) {
        TraceLog(LOG_WARNING, "CAMERA: Failed to initialize libcamera");
        delete g_camera_internal->wrapper;
        delete g_camera_internal;
        g_camera_internal = nullptr;
        return 0;
    }
    
    camera->initialized = 1;
    
    TraceLog(LOG_INFO, "CAMERA: Initialized successfully via libcamera");
    return 1;
#else
    TraceLog(LOG_INFO, "CAMERA: Not available on this platform");
    return 0;
#endif
}

int Camera_StartPreview(CameraSystem *camera) {
    if (camera == NULL || !camera->initialized) return 0;
    
#if defined(PLATFORM_RPI)
    if (!g_camera_internal || !g_camera_internal->wrapper) return 0;
    
    if (!g_camera_internal->wrapper->startPreview()) {
        TraceLog(LOG_WARNING, "CAMERA: Failed to start preview");
        return 0;
    }
    
    camera->preview_active = 1;
    TraceLog(LOG_INFO, "CAMERA: Preview started");
    return 1;
#else
    return 0;
#endif
}

void Camera_UpdatePreview(CameraSystem *camera) {
    if (camera == NULL || !camera->preview_active) return;
    
#if defined(PLATFORM_RPI)
    if (!g_camera_internal || !g_camera_internal->wrapper) return;
    
    // Note: Frame capture not yet implemented in libcamera wrapper
    // This is a placeholder for future implementation
    // Would capture frame and update texture here
#endif
}

int Camera_CapturePhoto(CameraSystem *camera, const char *filename) {
    if (camera == NULL || !camera->initialized || filename == NULL) return 0;
    
#if defined(PLATFORM_RPI)
    if (!g_camera_internal || !g_camera_internal->wrapper) return 0;
    
    // Create Photos directory using POSIX mkdir
    mkdir("Resources", 0755);
    mkdir("Resources/Photos", 0755);
    
    // WORKAROUND: Since the C++ libcamera API implementation is incomplete,
    // use libcamera-still command as a temporary solution
    // This captures a 150x150 square image directly
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
             "libcamera-still --immediate --nopreview --timeout 1 "
             "--width 150 --height 150 "
             "--roi 0.125,0.125,0.75,0.75 "
             "-o %s 2>/dev/null",
             filename);
    
    int ret = system(cmd);
    
    if (ret != 0 || access(filename, F_OK) != 0) {
        TraceLog(LOG_ERROR, "CAMERA: Failed to capture photo to %s", filename);
        return 0;
    }
    
    TraceLog(LOG_INFO, "CAMERA: Photo saved to %s", filename);
    return 1;
#else
    return 0;
#endif
}

void Camera_StopPreview(CameraSystem *camera) {
    if (camera == NULL) return;
    
#if defined(PLATFORM_RPI)
    if (camera->initialized && camera->preview_active) {
        if (g_camera_internal && g_camera_internal->wrapper) {
            g_camera_internal->wrapper->stopPreview();
        }
    }
    camera->preview_active = 0;
    TraceLog(LOG_INFO, "CAMERA: Preview stopped");
#endif
}

void Camera_Shutdown(CameraSystem *camera) {
    if (camera == NULL) return;
    
#if defined(PLATFORM_RPI)
    if (camera->initialized && g_camera_internal) {
        if (g_camera_internal->wrapper) {
            g_camera_internal->wrapper->shutdown();
            delete g_camera_internal->wrapper;
        }
        if (g_camera_internal->frame_buffer) {
            free(g_camera_internal->frame_buffer);
        }
        delete g_camera_internal;
        g_camera_internal = nullptr;
    }
    
    camera->initialized = 0;
    TraceLog(LOG_INFO, "CAMERA: Shutdown complete");
#endif
}
