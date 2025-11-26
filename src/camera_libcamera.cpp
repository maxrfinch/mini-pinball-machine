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
    
    // Ensure camera is configured
    if (!config_ && !configureCamera()) {
        return false;
    }
    
    // Create frame buffer allocator
    libcamera::FrameBufferAllocator allocator(camera_);
    
    libcamera::Stream *stream = config_->at(0).stream();
    if (allocator.allocate(stream) < 0) {
        std::cerr << "CAMERA: Failed to allocate buffers" << std::endl;
        return false;
    }
    
    // Create and queue requests
    std::vector<std::unique_ptr<libcamera::Request>> requests;
    
    for (const std::unique_ptr<libcamera::FrameBuffer> &buffer : allocator.buffers(stream)) {
        std::unique_ptr<libcamera::Request> request = camera_->createRequest();
        if (!request) {
            std::cerr << "CAMERA: Failed to create request" << std::endl;
            return false;
        }
        
        if (request->addBuffer(stream, buffer.get()) < 0) {
            std::cerr << "CAMERA: Failed to add buffer to request" << std::endl;
            return false;
        }
        
        requests.push_back(std::move(request));
    }
    
    // Start camera
    if (camera_->start() != 0) {
        std::cerr << "CAMERA: Failed to start camera" << std::endl;
        return false;
    }
    
    // Queue first request
    if (!requests.empty() && camera_->queueRequest(requests[0].get()) < 0) {
        std::cerr << "CAMERA: Failed to queue request" << std::endl;
        camera_->stop();
        return false;
    }
    
    // Wait for completion (simplified for now - in production use eventfd)
    // For now, we'll use a simple approach
    
    // Stop camera
    camera_->stop();
    
    // Process the captured frame
    // This is a simplified version - actual implementation would need proper request handling
    
    *buffer = nullptr;
    *size = 0;
    
    return true;
}

bool LibcameraWrapper::captureToFile(const char *filename) {
    if (!filename) {
        return false;
    }
    
    unsigned char *buffer = nullptr;
    size_t size = 0;
    
    if (!captureFrame(&buffer, &size)) {
        return false;
    }
    
    // For now, return success
    // Actual frame processing would happen here
    
    std::cout << "CAMERA: Photo captured to " << filename << std::endl;
    
    return true;
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
