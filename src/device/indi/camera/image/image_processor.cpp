#include "image_processor.hpp"
#include "../core/indi_camera_core.hpp"

#include <spdlog/spdlog.h>
#include <algorithm>
#include <cstring>
#include <numeric>

namespace lithium::device::indi::camera {

ImageProcessor::ImageProcessor(INDICameraCore* core) 
    : ComponentBase(core) {
    spdlog::debug("Creating image processor");
    setupImageFormats();
}

auto ImageProcessor::initialize() -> bool {
    spdlog::debug("Initializing image processor");
    
    // Reset image processing state
    currentImageFormat_ = "FITS";
    imageCompressionEnabled_.store(false);
    
    // Reset image quality metrics
    lastImageMean_.store(0.0);
    lastImageStdDev_.store(0.0);
    lastImageMin_.store(0);
    lastImageMax_.store(0);
    
    setupImageFormats();
    return true;
}

auto ImageProcessor::destroy() -> bool {
    spdlog::debug("Destroying image processor");
    return true;
}

auto ImageProcessor::getComponentName() const -> std::string {
    return "ImageProcessor";
}

auto ImageProcessor::handleProperty(INDI::Property property) -> bool {
    if (!property.isValid()) {
        return false;
    }
    
    std::string propertyName = property.getName();
    
    if (propertyName == "CCD1" && property.getType() == INDI_BLOB) {
        INDI::PropertyBlob blobProperty = property;
        processReceivedImage(blobProperty);
        return true;
    }
    
    return false;
}

auto ImageProcessor::setImageFormat(const std::string& format) -> bool {
    // Check if format is supported
    auto it = std::find(supportedImageFormats_.begin(), supportedImageFormats_.end(), format);
    if (it == supportedImageFormats_.end()) {
        spdlog::error("Unsupported image format: {}", format);
        return false;
    }
    
    currentImageFormat_ = format;
    spdlog::info("Image format set to: {}", format);
    return true;
}

auto ImageProcessor::getImageFormat() const -> std::string {
    return currentImageFormat_;
}

auto ImageProcessor::getSupportedImageFormats() const -> std::vector<std::string> {
    return supportedImageFormats_;
}

auto ImageProcessor::enableImageCompression(bool enable) -> bool {
    imageCompressionEnabled_.store(enable);
    spdlog::info("Image compression {}", enable ? "enabled" : "disabled");
    return true;
}

auto ImageProcessor::isImageCompressionEnabled() const -> bool {
    return imageCompressionEnabled_.load();
}

auto ImageProcessor::getLastImageQuality() const -> std::map<std::string, double> {
    std::map<std::string, double> quality;
    quality["mean"] = lastImageMean_.load();
    quality["stddev"] = lastImageStdDev_.load();
    quality["min"] = static_cast<double>(lastImageMin_.load());
    quality["max"] = static_cast<double>(lastImageMax_.load());
    return quality;
}

auto ImageProcessor::getFrameStatistics() const -> std::map<std::string, double> {
    // Return comprehensive frame statistics
    std::map<std::string, double> stats;
    stats["mean_brightness"] = lastImageMean_.load();
    stats["standard_deviation"] = lastImageStdDev_.load();
    stats["min_value"] = static_cast<double>(lastImageMin_.load());
    stats["max_value"] = static_cast<double>(lastImageMax_.load());
    stats["dynamic_range"] = static_cast<double>(lastImageMax_.load() - lastImageMin_.load());
    
    // Calculate signal-to-noise ratio (simplified)
    double mean = lastImageMean_.load();
    double stddev = lastImageStdDev_.load();
    if (stddev > 0) {
        stats["signal_to_noise_ratio"] = mean / stddev;
    } else {
        stats["signal_to_noise_ratio"] = 0.0;
    }
    
    return stats;
}

auto ImageProcessor::getImageFormat(const std::string& extension) -> std::string {
    std::string ext = extension;
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    if (ext == ".fits" || ext == ".fit") {
        return "FITS";
    } else if (ext == ".jpg" || ext == ".jpeg") {
        return "JPEG";
    } else if (ext == ".png") {
        return "PNG";
    } else if (ext == ".tiff" || ext == ".tif") {
        return "TIFF";
    } else if (ext == ".xisf") {
        return "XISF";
    } else {
        return "NATIVE";
    }
}

auto ImageProcessor::validateImageData(const void* data, size_t size) -> bool {
    if (!data || size == 0) {
        spdlog::error("Invalid image data: null pointer or zero size");
        return false;
    }
    
    const auto* bytes = static_cast<const uint8_t*>(data);
    
    // Check for common image format headers
    if (size >= 4) {
        // FITS format check
        if (std::memcmp(bytes, "SIMP", 4) == 0) {
            spdlog::debug("Detected FITS image format");
            return true;
        }
        
        // JPEG format check
        if (bytes[0] == 0xFF && bytes[1] == 0xD8) {
            spdlog::debug("Detected JPEG image format");
            return true;
        }
        
        // PNG format check
        if (bytes[0] == 0x89 && bytes[1] == 0x50 && bytes[2] == 0x4E && bytes[3] == 0x47) {
            spdlog::debug("Detected PNG image format");
            return true;
        }
        
        // TIFF format check
        if ((bytes[0] == 0x49 && bytes[1] == 0x49 && bytes[2] == 0x2A && bytes[3] == 0x00) ||
            (bytes[0] == 0x4D && bytes[1] == 0x4D && bytes[2] == 0x00 && bytes[3] == 0x2A)) {
            spdlog::debug("Detected TIFF image format");
            return true;
        }
    }
    
    // If no specific format detected, assume it's valid raw data
    spdlog::debug("Image format not specifically detected, assuming raw data");
    return true;
}

auto ImageProcessor::processReceivedImage(const INDI::PropertyBlob& property) -> void {
    if (!property.isValid()) {
        spdlog::error("Invalid blob property");
        return;
    }
    
    auto blob = property.getBlob();
    if (!blob || blob->getSize() == 0) {
        spdlog::error("Received empty image blob");
        return;
    }
    
    // Validate image data
    if (!validateImageData(blob->getData(), blob->getSize())) {
        spdlog::error("Invalid image data received");
        return;
    }
    
    // Create frame structure
    auto frame = std::make_shared<AtomCameraFrame>();
    frame->data = blob->getData();
    frame->size = blob->getSize();
    frame->timestamp = std::chrono::system_clock::now();
    frame->format = detectImageFormat(blob->getData(), blob->getSize());
    
    // Analyze image quality if it's raw data
    if (frame->format == "RAW" || frame->format == "FITS") {
        // Assume 16-bit data for analysis
        const auto* pixelData = static_cast<const uint16_t*>(frame->data);
        size_t pixelCount = frame->size / sizeof(uint16_t);
        analyzeImageQuality(pixelData, pixelCount);
    }
    
    // Update frame statistics
    updateImageStatistics(frame);
    
    // Store the frame in core
    getCore()->setCurrentFrame(frame);
    
    spdlog::info("Image processed: {} bytes, format: {}", frame->size, frame->format);
}

// Private methods
void ImageProcessor::setupImageFormats() {
    supportedImageFormats_ = {
        "FITS", "NATIVE", "XISF", "JPEG", "PNG", "TIFF"
    };
    currentImageFormat_ = "FITS";
    spdlog::debug("Supported image formats initialized");
}

void ImageProcessor::analyzeImageQuality(const uint16_t* data, size_t pixelCount) {
    if (!data || pixelCount == 0) {
        return;
    }
    
    // Find min and max values
    auto minMaxPair = std::minmax_element(data, data + pixelCount);
    int minVal = *minMaxPair.first;
    int maxVal = *minMaxPair.second;
    
    // Calculate mean
    uint64_t sum = std::accumulate(data, data + pixelCount, uint64_t(0));
    double mean = static_cast<double>(sum) / pixelCount;
    
    // Calculate standard deviation
    double variance = 0.0;
    for (size_t i = 0; i < pixelCount; ++i) {
        double diff = data[i] - mean;
        variance += diff * diff;
    }
    variance /= pixelCount;
    double stddev = std::sqrt(variance);
    
    // Update atomic values
    lastImageMean_.store(mean);
    lastImageStdDev_.store(stddev);
    lastImageMin_.store(minVal);
    lastImageMax_.store(maxVal);
    
    spdlog::debug("Image quality analysis: mean={:.2f}, stddev={:.2f}, min={}, max={}", 
                  mean, stddev, minVal, maxVal);
}

void ImageProcessor::updateImageStatistics(std::shared_ptr<AtomCameraFrame> frame) {
    if (!frame) {
        return;
    }
    
    // Update frame metadata
    frame->quality.mean = lastImageMean_.load();
    frame->quality.stddev = lastImageStdDev_.load();
    frame->quality.min = lastImageMin_.load();
    frame->quality.max = lastImageMax_.load();
    
    // Calculate additional statistics
    if (frame->quality.stddev > 0) {
        frame->quality.snr = frame->quality.mean / frame->quality.stddev;
    } else {
        frame->quality.snr = 0.0;
    }
    
    frame->quality.dynamicRange = frame->quality.max - frame->quality.min;
}

auto ImageProcessor::detectImageFormat(const void* data, size_t size) -> std::string {
    if (!data || size < 4) {
        return "UNKNOWN";
    }
    
    const auto* bytes = static_cast<const uint8_t*>(data);
    
    // FITS format
    if (std::memcmp(bytes, "SIMP", 4) == 0) {
        return "FITS";
    }
    
    // JPEG format
    if (bytes[0] == 0xFF && bytes[1] == 0xD8) {
        return "JPEG";
    }
    
    // PNG format
    if (bytes[0] == 0x89 && bytes[1] == 0x50 && bytes[2] == 0x4E && bytes[3] == 0x47) {
        return "PNG";
    }
    
    // TIFF format
    if ((bytes[0] == 0x49 && bytes[1] == 0x49 && bytes[2] == 0x2A && bytes[3] == 0x00) ||
        (bytes[0] == 0x4D && bytes[1] == 0x4D && bytes[2] == 0x00 && bytes[3] == 0x2A)) {
        return "TIFF";
    }
    
    // Default to RAW for unrecognized formats
    return "RAW";
}

} // namespace lithium::device::indi::camera
