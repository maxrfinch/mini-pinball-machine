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
    
    // JPEG frame parsing state (moved here for thread safety)
    unsigned char jpeg_buffer[64 * 1024];  // 64KB buffer for JPEG frames
    size_t jpeg_size;
    int in_frame;
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

// Helper to terminate a child process with proper cleanup
// Waits up to timeout_ms for graceful termination before force killing
static void terminate_child_process(pid_t pid, int timeout_ms) {
    if (pid <= 0) return;
    
    // First try SIGUSR2 (graceful stop for libcamera)
    kill(pid, SIGUSR2);
    
    // Wait for process to exit with timeout
    int waited = 0;
    int status;
    while (waited < timeout_ms) {
        pid_t result = waitpid(pid, &status, WNOHANG);
        if (result == pid) {
            return;  // Process exited
        }
        if (result == -1) {
            return;  // Error (process doesn't exist)
        }
        usleep(10000);  // 10ms
        waited += 10;
    }
    
    // Try SIGTERM
    kill(pid, SIGTERM);
    usleep(50000);  // 50ms grace period
    
    // Check again
    if (waitpid(pid, &status, WNOHANG) == pid) {
        return;
    }
    
    // Force kill with SIGKILL
    kill(pid, SIGKILL);
    waitpid(pid, &status, 0);  // Blocking wait to reap zombie
}

// Helper to validate filename contains only safe characters
static int is_safe_filename(const char *filename) {
    if (!filename || !*filename) return 0;
    
    // Check for path traversal
    if (strstr(filename, "..")) return 0;
    
    // Allow alphanumeric, underscore, hyphen, dot, and forward slash
    for (const char *p = filename; *p; p++) {
        char c = *p;
        if (!((c >= 'a' && c <= 'z') || 
              (c >= 'A' && c <= 'Z') || 
              (c >= '0' && c <= '9') ||
              c == '_' || c == '-' || c == '.' || c == '/')) {
            return 0;
        }
    }
    return 1;
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
    g_camera_internal->jpeg_size = 0;
    g_camera_internal->in_frame = 0;
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
    
    // Read JPEG frame data from pipe using per-instance state (thread-safe)
    unsigned char byte;
    while (read(g_camera_internal->preview_pipe_fd, &byte, 1) == 1) {
        // Look for JPEG start marker (0xFF 0xD8)
        if (!g_camera_internal->in_frame && 
            g_camera_internal->jpeg_size > 0 && 
            g_camera_internal->jpeg_buffer[g_camera_internal->jpeg_size - 1] == 0xFF && 
            byte == 0xD8) {
            // Found start of new frame - store both marker bytes
            g_camera_internal->jpeg_buffer[0] = 0xFF;
            g_camera_internal->jpeg_buffer[1] = 0xD8;
            g_camera_internal->jpeg_size = 2;
            g_camera_internal->in_frame = 1;
            continue;  // Don't add 0xD8 again below
        }
        
        // Check for buffer overflow - reset frame parsing if buffer full
        if (g_camera_internal->jpeg_size >= sizeof(g_camera_internal->jpeg_buffer)) {
            g_camera_internal->jpeg_size = 0;
            g_camera_internal->in_frame = 0;
            continue;
        }
        
        g_camera_internal->jpeg_buffer[g_camera_internal->jpeg_size++] = byte;
        
        // Look for JPEG end marker (0xFF 0xD9)
        if (g_camera_internal->in_frame && 
            g_camera_internal->jpeg_size >= 2 && 
            g_camera_internal->jpeg_buffer[g_camera_internal->jpeg_size - 2] == 0xFF && 
            byte == 0xD9) {
            // Complete frame received - decode it
            Image img = LoadImageFromMemory(".jpg", g_camera_internal->jpeg_buffer, 
                                           (int)g_camera_internal->jpeg_size);
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
            g_camera_internal->jpeg_size = 0;
            g_camera_internal->in_frame = 0;
        }
    }
#endif
}

int Camera_CapturePhoto(CameraSystem *camera, const char *filename) {
    if (camera == NULL || !camera->initialized || filename == NULL) return 0;
    
#if defined(PLATFORM_RPI)
    if (!g_camera_internal) return 0;
    
    // Validate filename for safety
    if (!is_safe_filename(filename)) {
        TraceLog(LOG_ERROR, "CAMERA: Invalid filename: %s", filename);
        return 0;
    }
    
    // Stop the preview process temporarily to free the camera for capture
    int was_previewing = camera->preview_active;
    if (was_previewing) {
        // Stop preview process with proper cleanup
        if (g_camera_internal->preview_pid > 0) {
            terminate_child_process(g_camera_internal->preview_pid, 200);
            g_camera_internal->preview_pid = -1;
        }
        if (g_camera_internal->preview_pipe_fd >= 0) {
            close(g_camera_internal->preview_pipe_fd);
            g_camera_internal->preview_pipe_fd = -1;
        }
        usleep(100000);  // Wait 100ms for camera to be fully released
    }
    
    // Create Photos directory using POSIX mkdir
    mkdir("Resources", 0755);
    mkdir("Resources/Photos", 0755);
    
    // Determine output format from filename extension
    const char *ext = strrchr(filename, '.');
    int is_png = (ext && strcasecmp(ext, ".png") == 0);
    
    // Create secure temporary file for JPEG capture
    char temp_file[512];
    int temp_fd = -1;
    
    if (is_png) {
        // Use mkstemp for secure temp file creation
        snprintf(temp_file, sizeof(temp_file), "/tmp/pinball_capture_XXXXXX");
        temp_fd = mkstemp(temp_file);
        if (temp_fd == -1) {
            TraceLog(LOG_ERROR, "CAMERA: Failed to create temp file: %s", strerror(errno));
            if (was_previewing) {
                Camera_StartPreview(camera);
            }
            return 0;
        }
        close(temp_fd);  // Close immediately, we just need the unique name
        
        // Append .jpg extension
        char temp_file_jpg[520];
        snprintf(temp_file_jpg, sizeof(temp_file_jpg), "%s.jpg", temp_file);
        rename(temp_file, temp_file_jpg);
        strncpy(temp_file, temp_file_jpg, sizeof(temp_file) - 1);
        temp_file[sizeof(temp_file) - 1] = '\0';
    }
    
    // Build command - temp_file is safe (from mkstemp), filename is validated
    char cmd[1024];
    const char *output_file = is_png ? temp_file : filename;
    snprintf(cmd, sizeof(cmd),
             "libcamera-still --immediate --nopreview --timeout 500 "
             "--width 640 --height 480 "
             "-e jpg "
             "-o %s 2>/dev/null",
             output_file);
    
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
        // Stop preview process with proper cleanup
        if (g_camera_internal->preview_pid > 0) {
            terminate_child_process(g_camera_internal->preview_pid, 150);
            g_camera_internal->preview_pid = -1;
        }
        
        // Close pipe
        if (g_camera_internal->preview_pipe_fd >= 0) {
            close(g_camera_internal->preview_pipe_fd);
            g_camera_internal->preview_pipe_fd = -1;
        }
        
        // Remove pipe file
        unlink(g_camera_internal->preview_pipe_path);
        
        // Reset JPEG parsing state
        g_camera_internal->jpeg_size = 0;
        g_camera_internal->in_frame = 0;
        
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
