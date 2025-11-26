#ifndef CAMERA_H
#define CAMERA_H

#include "raylib.h"

#ifdef __cplusplus
extern "C" {
#endif

// Camera system for Pi Camera Module 3
// Provides live preview and photo capture functionality
// On non-Pi platforms, these are no-op stubs

typedef struct {
    int initialized;        // 1 if camera is active, 0 otherwise
    int preview_active;     // 1 if preview is running, 0 otherwise
    Texture2D preview_tex;  // Texture for live preview display
    int preview_width;      // Width of preview (150)
    int preview_height;     // Height of preview (150)
    float zoom_factor;      // Zoom factor for cropping (default 1.0)
} CameraSystem;

// Initialize camera system (Pi only, no-op on other platforms)
// Returns 1 on success, 0 on failure
int Camera_Init(CameraSystem *camera);

// Start live preview for Top-3 Game Over screen
// Returns 1 on success, 0 on failure
int Camera_StartPreview(CameraSystem *camera);

// Update preview texture with latest frame
// Call this each frame to refresh the live preview
void Camera_UpdatePreview(CameraSystem *camera);

// Capture a photo and save to disk
// filename: Path to save the photo (e.g., "Resources/Photos/NAME_SCORE.png")
// Returns 1 on success, 0 on failure
int Camera_CapturePhoto(CameraSystem *camera, const char *filename);

// Stop live preview
void Camera_StopPreview(CameraSystem *camera);

// Shutdown camera system and free resources
void Camera_Shutdown(CameraSystem *camera);

#ifdef __cplusplus
}
#endif

#endif // CAMERA_H
