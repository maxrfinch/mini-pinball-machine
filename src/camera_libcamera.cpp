#include "camera_libcamera.hpp"

#if defined(PLATFORM_RPI)

#include <iostream>
#include <fstream>
#include <cstring>
#include <jpeglib.h>

LibcameraWrapper::LibcameraWrapper() 
    : initialized_(false), preview_active_(false) {
}

LibcameraWrapper::~LibcameraWrapper() {
    shutdown();
}

bool LibcameraWrapper::init() {
    if (initialized_) {
        return true;
    }
    
    // Create camera manager
    camera_manager_ = std::make_unique<libcamera::CameraManager>();
    
    int ret = camera_manager_->start();
    if (ret != 0) {
        std::cerr << "CAMERA: Failed to start camera manager" << std::endl;
        return false;
    }
    
    // Get first available camera
    if (camera_manager_->cameras().empty()) {
        std::cerr << "CAMERA: No cameras detected" << std::endl;
        camera_manager_->stop();
        return false;
    }
    
    camera_ = camera_manager_->cameras()[0];
    
    // Acquire camera
    if (camera_->acquire() != 0) {
        std::cerr << "CAMERA: Failed to acquire camera" << std::endl;
        camera_manager_->stop();
        return false;
    }
    
    initialized_ = true;
    std::cout << "CAMERA: Initialized successfully (Camera: " 
              << camera_->id() << ")" << std::endl;
    
    return true;
}

bool LibcameraWrapper::configureCamera() {
    if (!initialized_ || !camera_) {
        return false;
    }
    
    // Generate configuration for still capture
    config_ = camera_->generateConfiguration({libcamera::StreamRole::StillCapture});
    
    if (!config_) {
        std::cerr << "CAMERA: Failed to generate configuration" << std::endl;
        return false;
    }
    
    // Configure stream for 150x150 square output
    libcamera::StreamConfiguration &streamConfig = config_->at(0);
    streamConfig.size = libcamera::Size(640, 480);  // Start with larger size for quality
    streamConfig.pixelFormat = libcamera::formats::RGB888;
    
    // Validate configuration
    libcamera::CameraConfiguration::Status status = config_->validate();
    if (status == libcamera::CameraConfiguration::Invalid) {
        std::cerr << "CAMERA: Invalid configuration" << std::endl;
        return false;
    }
    
    // Apply configuration
    if (camera_->configure(config_.get()) < 0) {
        std::cerr << "CAMERA: Failed to apply configuration" << std::endl;
        return false;
    }
    
    return true;
}

bool LibcameraWrapper::startPreview() {
    if (!initialized_ || preview_active_) {
        return false;
    }
    
    if (!configureCamera()) {
        return false;
    }
    
    preview_active_ = true;
    std::cout << "CAMERA: Preview started" << std::endl;
    
    return true;
}

bool LibcameraWrapper::captureFrame(unsigned char **buffer, size_t *size) {
    if (!initialized_ || !camera_) {
        return false;
    }
    
    // TODO: Full implementation needed
    // This skeleton shows the structure but doesn't actually capture frames yet.
    // Full implementation requires:
    // 1. Proper request/completion handling with event loop
    // 2. Frame buffer mapping and memory access
    // 3. Format conversion (RGB/YUV to target format)
    // 4. JPEG encoding if needed
    
    std::cerr << "CAMERA: captureFrame() not yet fully implemented" << std::endl;
    
    *buffer = nullptr;
    *size = 0;
    
    return false;  // Not yet implemented
}

bool LibcameraWrapper::captureToFile(const char *filename) {
    if (!filename) {
        return false;
    }
    
    // TODO: Full implementation needed
    // For a working implementation, this would:
    // 1. Configure camera for still capture
    // 2. Capture a frame using proper request handling
    // 3. Convert/encode the frame data
    // 4. Write to file
    //
    // For now, use libcamera-still as external command as a workaround
    // (see camera_wrapper.cpp comment about using system commands)
    
    std::cerr << "CAMERA: captureToFile() not yet fully implemented" << std::endl;
    std::cerr << "CAMERA: Consider using libcamera-still external command" << std::endl;
    
    return false;  // Not yet implemented
}

void LibcameraWrapper::stopPreview() {
    if (!preview_active_) {
        return;
    }
    
    preview_active_ = false;
    std::cout << "CAMERA: Preview stopped" << std::endl;
}

void LibcameraWrapper::shutdown() {
    if (!initialized_) {
        return;
    }
    
    stopPreview();
    
    if (camera_) {
        camera_->release();
        camera_.reset();
    }
    
    if (camera_manager_) {
        camera_manager_->stop();
        camera_manager_.reset();
    }
    
    initialized_ = false;
    std::cout << "CAMERA: Shutdown complete" << std::endl;
}

#endif // PLATFORM_RPI
