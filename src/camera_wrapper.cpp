#include "camera.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

#if defined(PLATFORM_RPI)
#include "camera_libcamera.hpp"
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>

// Internal C++ instance (opaque to C code)
struct CameraSystemInternal {
    LibcameraWrapper *wrapper;
    unsigned char *frame_buffer;
    size_t frame_size;
    
    // Live preview process
    pid_t preview_pid;
    char preview_pipe_path[256];
    int preview_pipe_fd;
    unsigned char *preview_buffer;
    size_t preview_buffer_size;
    Image preview_image;
    double last_frame_time;
};

#endif

// C API Implementation

#if defined(PLATFORM_RPI)
// Storage for internal camera instance
static CameraSystemInternal *g_camera_internal = nullptr;

// Helper to check if camera is available
static int check_camera_available(void) {
    int ret = system("libcamera-hello --list-cameras 2>/dev/null | grep -q 'Available cameras'");
    return (ret == 0) ? 1 : 0;
}
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
    // Check if camera is available using libcamera-hello
    if (!check_camera_available()) {
        TraceLog(LOG_WARNING, "CAMERA: No camera detected");
        return 0;
    }
    
    // Clean up any existing instance
    if (g_camera_internal != nullptr) {
        if (g_camera_internal->wrapper) {
            delete g_camera_internal->wrapper;
        }
        if (g_camera_internal->preview_buffer) {
            free(g_camera_internal->preview_buffer);
        }
        delete g_camera_internal;
        g_camera_internal = nullptr;
    }
    
    g_camera_internal = new CameraSystemInternal();
    g_camera_internal->wrapper = new LibcameraWrapper();
    g_camera_internal->frame_buffer = nullptr;
    g_camera_internal->frame_size = 0;
    g_camera_internal->preview_pid = -1;
    g_camera_internal->preview_pipe_fd = -1;
    g_camera_internal->preview_buffer = nullptr;
    g_camera_internal->preview_buffer_size = 0;
    g_camera_internal->preview_image = (Image){0};
    g_camera_internal->last_frame_time = 0;
    snprintf(g_camera_internal->preview_pipe_path, sizeof(g_camera_internal->preview_pipe_path),
             "/tmp/pinball_camera_preview_%d", getpid());
    
    if (!g_camera_internal->wrapper->init()) {
        TraceLog(LOG_WARNING, "CAMERA: Failed to initialize libcamera wrapper, using command fallback");
        // Continue anyway - we can still use command-line tools
    }
    
    camera->initialized = 1;
    
    TraceLog(LOG_INFO, "CAMERA: Initialized successfully");
    return 1;
#else
    TraceLog(LOG_INFO, "CAMERA: Not available on this platform");
    return 0;
#endif
}

int Camera_StartPreview(CameraSystem *camera) {
    if (camera == NULL || !camera->initialized) return 0;
    
#if defined(PLATFORM_RPI)
    if (!g_camera_internal) return 0;
    
    // Stop any existing preview
    if (camera->preview_active) {
        Camera_StopPreview(camera);
    }
    
    // Remove any existing pipe
    unlink(g_camera_internal->preview_pipe_path);
    
    // Create named pipe for preview frames
    if (mkfifo(g_camera_internal->preview_pipe_path, 0600) != 0 && errno != EEXIST) {
        TraceLog(LOG_ERROR, "CAMERA: Failed to create preview pipe: %s", strerror(errno));
        return 0;
    }
    
    // Start libcamera-vid to capture MJPEG frames to the pipe
    pid_t pid = fork();
    if (pid == -1) {
        TraceLog(LOG_ERROR, "CAMERA: Failed to fork for preview: %s", strerror(errno));
        unlink(g_camera_internal->preview_pipe_path);
        return 0;
    }
    
    if (pid == 0) {
        // Child process - exec libcamera-vid
        // Output MJPEG frames at 15fps, 320x240 (we'll crop/scale)
        // Use --signal to allow stopping with SIGUSR2
        execlp("libcamera-vid", "libcamera-vid",
               "-t", "0",                    // Run indefinitely
               "--width", "320",
               "--height", "240",
               "--framerate", "15",
               "--codec", "mjpeg",
               "-o", g_camera_internal->preview_pipe_path,
               "--nopreview",
               "--signal",                   // Exit on SIGUSR2
               NULL);
        // If exec fails, exit child
        _exit(1);
    }
    
    // Parent process
    g_camera_internal->preview_pid = pid;
    
    // Open the pipe for reading (non-blocking)
    int flags = O_RDONLY | O_NONBLOCK;
    g_camera_internal->preview_pipe_fd = open(g_camera_internal->preview_pipe_path, flags);
    if (g_camera_internal->preview_pipe_fd == -1) {
        TraceLog(LOG_WARNING, "CAMERA: Failed to open preview pipe (will retry): %s", strerror(errno));
        // Don't fail - the pipe will be available when libcamera-vid starts writing
    }
    
    // Allocate preview buffer (enough for one JPEG frame)
    g_camera_internal->preview_buffer_size = 320 * 240 * 3;  // RGB buffer size
    g_camera_internal->preview_buffer = (unsigned char*)malloc(g_camera_internal->preview_buffer_size);
    
    camera->preview_active = 1;
    g_camera_internal->last_frame_time = GetTime();
    TraceLog(LOG_INFO, "CAMERA: Preview started (pid=%d)", pid);
    return 1;
#else
    return 0;
#endif
}

void Camera_UpdatePreview(CameraSystem *camera) {
    if (camera == NULL || !camera->preview_active) return;
    
#if defined(PLATFORM_RPI)
    if (!g_camera_internal) return;
    
    // Try to open pipe if not yet open
    if (g_camera_internal->preview_pipe_fd == -1) {
        g_camera_internal->preview_pipe_fd = open(g_camera_internal->preview_pipe_path, 
                                                   O_RDONLY | O_NONBLOCK);
        if (g_camera_internal->preview_pipe_fd == -1) {
            return;  // Not ready yet
        }
    }
    
    // Read JPEG frame data from pipe
    static unsigned char jpeg_buffer[64 * 1024];  // 64KB should be enough for a frame
    static size_t jpeg_size = 0;
    static int in_frame = 0;
    
    unsigned char byte;
    while (read(g_camera_internal->preview_pipe_fd, &byte, 1) == 1) {
        // Look for JPEG start marker (0xFF 0xD8)
        if (!in_frame && jpeg_size > 0 && jpeg_buffer[jpeg_size - 1] == 0xFF && byte == 0xD8) {
            // Found start of new frame - reset
            jpeg_size = 1;
            jpeg_buffer[0] = 0xFF;
            in_frame = 1;
        }
        
        if (jpeg_size < sizeof(jpeg_buffer)) {
            jpeg_buffer[jpeg_size++] = byte;
        }
        
        // Look for JPEG end marker (0xFF 0xD9)
        if (in_frame && jpeg_size >= 2 && jpeg_buffer[jpeg_size - 2] == 0xFF && byte == 0xD9) {
            // Complete frame received - decode it
            Image img = LoadImageFromMemory(".jpg", jpeg_buffer, jpeg_size);
            if (img.data != NULL) {
                // Crop to square (center crop)
                int cropSize = (img.width < img.height) ? img.width : img.height;
                int cropX = (img.width - cropSize) / 2;
                int cropY = (img.height - cropSize) / 2;
                ImageCrop(&img, (Rectangle){(float)cropX, (float)cropY, (float)cropSize, (float)cropSize});
                
                // Resize to 150x150
                ImageResize(&img, camera->preview_width, camera->preview_height);
                
                // Update texture
                if (camera->preview_tex.id != 0) {
                    UnloadTexture(camera->preview_tex);
                }
                camera->preview_tex = LoadTextureFromImage(img);
                UnloadImage(img);
                
                g_camera_internal->last_frame_time = GetTime();
            }
            
            // Reset for next frame
            jpeg_size = 0;
            in_frame = 0;
        }
    }
#endif
}

int Camera_CapturePhoto(CameraSystem *camera, const char *filename) {
    if (camera == NULL || !camera->initialized || filename == NULL) return 0;
    
#if defined(PLATFORM_RPI)
    if (!g_camera_internal) return 0;
    
    // Stop the preview process temporarily to free the camera for capture
    int was_previewing = camera->preview_active;
    if (was_previewing) {
        // Stop preview process
        if (g_camera_internal->preview_pid > 0) {
            kill(g_camera_internal->preview_pid, SIGUSR2);  // Signal to stop
            usleep(100000);  // Wait 100ms for process to stop
            kill(g_camera_internal->preview_pid, SIGTERM);  // Force stop if needed
            waitpid(g_camera_internal->preview_pid, NULL, WNOHANG);
            g_camera_internal->preview_pid = -1;
        }
        if (g_camera_internal->preview_pipe_fd >= 0) {
            close(g_camera_internal->preview_pipe_fd);
            g_camera_internal->preview_pipe_fd = -1;
        }
        usleep(200000);  // Wait 200ms for camera to be fully released
    }
    
    // Create Photos directory using POSIX mkdir
    mkdir("Resources", 0755);
    mkdir("Resources/Photos", 0755);
    
    // Determine output format from filename extension
    const char *ext = strrchr(filename, '.');
    int is_png = (ext && strcasecmp(ext, ".png") == 0);
    
    // Capture using libcamera-still
    // Use a temporary JPEG file, then convert to PNG if needed
    char temp_file[512];
    char cmd[1024];
    
    if (is_png) {
        // Capture to temp JPEG first, then convert
        snprintf(temp_file, sizeof(temp_file), "/tmp/pinball_capture_%d.jpg", getpid());
        snprintf(cmd, sizeof(cmd),
                 "libcamera-still --immediate --nopreview --timeout 500 "
                 "--width 640 --height 480 "
                 "-e jpg "
                 "-o '%s' 2>/dev/null",
                 temp_file);
    } else {
        // Direct JPEG output
        snprintf(cmd, sizeof(cmd),
                 "libcamera-still --immediate --nopreview --timeout 500 "
                 "--width 640 --height 480 "
                 "-e jpg "
                 "-o '%s' 2>/dev/null",
                 filename);
    }
    
    TraceLog(LOG_DEBUG, "CAMERA: Running capture command: %s", cmd);
    int ret = system(cmd);
    
    const char *captured_file = is_png ? temp_file : filename;
    if (ret != 0 || access(captured_file, F_OK) != 0) {
        TraceLog(LOG_ERROR, "CAMERA: Failed to capture photo (ret=%d)", ret);
        if (was_previewing) {
            Camera_StartPreview(camera);  // Restart preview
        }
        return 0;
    }
    
    // If PNG requested, load the JPEG, crop to square, and save as PNG
    if (is_png) {
        Image img = LoadImage(temp_file);
        if (img.data == NULL) {
            TraceLog(LOG_ERROR, "CAMERA: Failed to load captured image");
            unlink(temp_file);
            if (was_previewing) {
                Camera_StartPreview(camera);
            }
            return 0;
        }
        
        // Crop to center square
        int cropSize = (img.width < img.height) ? img.width : img.height;
        int cropX = (img.width - cropSize) / 2;
        int cropY = (img.height - cropSize) / 2;
        ImageCrop(&img, (Rectangle){(float)cropX, (float)cropY, (float)cropSize, (float)cropSize});
        
        // Resize to 150x150
        ImageResize(&img, 150, 150);
        
        // Save as PNG
        if (!ExportImage(img, filename)) {
            TraceLog(LOG_ERROR, "CAMERA: Failed to save PNG to %s", filename);
            UnloadImage(img);
            unlink(temp_file);
            if (was_previewing) {
                Camera_StartPreview(camera);
            }
            return 0;
        }
        
        UnloadImage(img);
        unlink(temp_file);  // Clean up temp file
    }
    
    // Restart preview if it was active
    if (was_previewing) {
        Camera_StartPreview(camera);
    }
    
    if (access(filename, F_OK) != 0) {
        TraceLog(LOG_ERROR, "CAMERA: Photo file not created: %s", filename);
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
    if (camera->preview_active && g_camera_internal) {
        // Stop preview process
        if (g_camera_internal->preview_pid > 0) {
            kill(g_camera_internal->preview_pid, SIGUSR2);  // Signal to stop
            usleep(50000);  // Wait 50ms
            kill(g_camera_internal->preview_pid, SIGTERM);  // Force stop
            waitpid(g_camera_internal->preview_pid, NULL, WNOHANG);
            g_camera_internal->preview_pid = -1;
        }
        
        // Close pipe
        if (g_camera_internal->preview_pipe_fd >= 0) {
            close(g_camera_internal->preview_pipe_fd);
            g_camera_internal->preview_pipe_fd = -1;
        }
        
        // Remove pipe file
        unlink(g_camera_internal->preview_pipe_path);
        
        // Unload preview texture
        if (camera->preview_tex.id != 0) {
            UnloadTexture(camera->preview_tex);
            camera->preview_tex = (Texture2D){0};
        }
        
        if (g_camera_internal->wrapper) {
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
    // Stop preview first
    Camera_StopPreview(camera);
    
    if (camera->initialized && g_camera_internal) {
        if (g_camera_internal->wrapper) {
            g_camera_internal->wrapper->shutdown();
            delete g_camera_internal->wrapper;
        }
        if (g_camera_internal->frame_buffer) {
            free(g_camera_internal->frame_buffer);
        }
        if (g_camera_internal->preview_buffer) {
            free(g_camera_internal->preview_buffer);
        }
        delete g_camera_internal;
        g_camera_internal = nullptr;
    }
    
    camera->initialized = 0;
    TraceLog(LOG_INFO, "CAMERA: Shutdown complete");
#endif
}
