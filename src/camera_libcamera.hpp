#ifndef CAMERA_LIBCAMERA_HPP
#define CAMERA_LIBCAMERA_HPP

// C++ wrapper for libcamera API
// This provides a clean interface for camera operations on Raspberry Pi

#if defined(PLATFORM_RPI)

#include <memory>
#include <string>
#include <libcamera/libcamera.h>

class LibcameraWrapper {
public:
    LibcameraWrapper();
    ~LibcameraWrapper();

    // Initialize camera system
    bool init();
    
    // Start preview mode
    bool startPreview();
    
    // Capture a frame to memory buffer (JPEG format)
    // Returns pointer to buffer and size
    bool captureFrame(unsigned char **buffer, size_t *size);
    
    // Capture a frame and save to file
    bool captureToFile(const char *filename);
    
    // Stop preview
    void stopPreview();
    
    // Shutdown camera
    void shutdown();
    
    // Check if camera is initialized
    bool isInitialized() const { return initialized_; }
    
    // Check if preview is active
    bool isPreviewActive() const { return preview_active_; }

private:
    bool initialized_;
    bool preview_active_;
    
    std::unique_ptr<libcamera::CameraManager> camera_manager_;
    std::shared_ptr<libcamera::Camera> camera_;
    std::unique_ptr<libcamera::CameraConfiguration> config_;
    
    // Helper to configure camera for square 150x150 capture
    bool configureCamera();
    
    // Helper to process captured frame
    bool processRequest(libcamera::Request *request, unsigned char **buffer, size_t *size);
};

#endif // PLATFORM_RPI

#endif // CAMERA_LIBCAMERA_HPP
