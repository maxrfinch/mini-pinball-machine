#include "camera.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

int Camera_Init(CameraSystem *camera) {
    if (camera == NULL) return 0;
    
    camera->initialized = 0;
    camera->preview_active = 0;
    camera->preview_tex = (Texture2D){0};
    camera->preview_width = 150;
    camera->preview_height = 150;
    camera->zoom_factor = 1.0f;
    
#if defined(PLATFORM_RPI)
    CameraSystemInternal *internal = new CameraSystemInternal();
    internal->wrapper = new LibcameraWrapper();
    internal->frame_buffer = nullptr;
    internal->frame_size = 0;
    
    if (!internal->wrapper->init()) {
        TraceLog(LOG_WARNING, "CAMERA: Failed to initialize libcamera");
        delete internal->wrapper;
        delete internal;
        return 0;
    }
    
    // Store internal pointer in unused texture ID field (hack but works)
    camera->preview_tex.id = (unsigned int)(uintptr_t)internal;
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
    CameraSystemInternal *internal = (CameraSystemInternal*)(uintptr_t)camera->preview_tex.id;
    if (!internal || !internal->wrapper) return 0;
    
    if (!internal->wrapper->startPreview()) {
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
    CameraSystemInternal *internal = (CameraSystemInternal*)(uintptr_t)camera->preview_tex.id;
    if (!internal || !internal->wrapper) return;
    
    // Capture a frame
    unsigned char *buffer = nullptr;
    size_t size = 0;
    
    if (internal->wrapper->captureFrame(&buffer, &size) && buffer && size > 0) {
        // Update texture from buffer
        // For now, we'll skip this as the libcamera implementation needs more work
        // In production, would decode JPEG and update texture
    }
#endif
}

int Camera_CapturePhoto(CameraSystem *camera, const char *filename) {
    if (camera == NULL || !camera->initialized || filename == NULL) return 0;
    
#if defined(PLATFORM_RPI)
    CameraSystemInternal *internal = (CameraSystemInternal*)(uintptr_t)camera->preview_tex.id;
    if (!internal || !internal->wrapper) return 0;
    
    // Ensure Photos directory exists
    system("mkdir -p Resources/Photos");
    
    if (!internal->wrapper->captureToFile(filename)) {
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
        CameraSystemInternal *internal = (CameraSystemInternal*)(uintptr_t)camera->preview_tex.id;
        if (internal && internal->wrapper) {
            internal->wrapper->stopPreview();
        }
    }
    camera->preview_active = 0;
    TraceLog(LOG_INFO, "CAMERA: Preview stopped");
#endif
}

void Camera_Shutdown(CameraSystem *camera) {
    if (camera == NULL) return;
    
#if defined(PLATFORM_RPI)
    if (camera->initialized) {
        CameraSystemInternal *internal = (CameraSystemInternal*)(uintptr_t)camera->preview_tex.id;
        if (internal) {
            if (internal->wrapper) {
                internal->wrapper->shutdown();
                delete internal->wrapper;
            }
            if (internal->frame_buffer) {
                free(internal->frame_buffer);
            }
            delete internal;
        }
    }
    
    camera->initialized = 0;
    camera->preview_tex.id = 0;
    TraceLog(LOG_INFO, "CAMERA: Shutdown complete");
#endif
}
