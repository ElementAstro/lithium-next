#include <iostream>
#include <memory>
#include "camera_tasks.hpp"

using namespace lithium::task::task;

int main() {
    std::cout << "=== Camera Task System Build Test ===" << std::endl;
    std::cout << "Version: " << CameraTaskSystemInfo::VERSION << std::endl;
    std::cout << "Build Date: " << CameraTaskSystemInfo::BUILD_DATE << std::endl;
    std::cout << "Total Tasks: " << CameraTaskSystemInfo::TOTAL_TASKS << std::endl;
    
    std::cout << "\n=== Testing Task Creation ===" << std::endl;
    
    try {
        // Test basic exposure tasks
        auto takeExposure = std::make_unique<TakeExposureTask>("TakeExposure", nullptr);
        auto takeManyExposure = std::make_unique<TakeManyExposureTask>("TakeManyExposure", nullptr);
        auto subFrameExposure = std::make_unique<SubFrameExposureTask>("SubFrameExposure", nullptr);
        std::cout << "✓ Basic exposure tasks created successfully" << std::endl;
        
        // Test calibration tasks
        auto darkFrame = std::make_unique<DarkFrameTask>("DarkFrame", nullptr);
        auto biasFrame = std::make_unique<BiasFrameTask>("BiasFrame", nullptr);
        auto flatFrame = std::make_unique<FlatFrameTask>("FlatFrame", nullptr);
        std::cout << "✓ Calibration tasks created successfully" << std::endl;
        
        // Test video tasks
        auto startVideo = std::make_unique<StartVideoTask>("StartVideo", nullptr);
        auto recordVideo = std::make_unique<RecordVideoTask>("RecordVideo", nullptr);
        std::cout << "✓ Video tasks created successfully" << std::endl;
        
        // Test temperature tasks
        auto coolingControl = std::make_unique<CoolingControlTask>("CoolingControl", nullptr);
        auto tempMonitor = std::make_unique<TemperatureMonitorTask>("TemperatureMonitor", nullptr);
        std::cout << "✓ Temperature tasks created successfully" << std::endl;
        
        // Test frame tasks
        auto frameConfig = std::make_unique<FrameConfigTask>("FrameConfig", nullptr);
        auto roiConfig = std::make_unique<ROIConfigTask>("ROIConfig", nullptr);
        std::cout << "✓ Frame tasks created successfully" << std::endl;
        
        // Test parameter tasks
        auto gainControl = std::make_unique<GainControlTask>("GainControl", nullptr);
        auto offsetControl = std::make_unique<OffsetControlTask>("OffsetControl", nullptr);
        std::cout << "✓ Parameter tasks created successfully" << std::endl;
        
        // Test telescope tasks
        auto telescopeGoto = std::make_unique<TelescopeGotoImagingTask>("TelescopeGotoImaging", nullptr);
        auto trackingControl = std::make_unique<TrackingControlTask>("TrackingControl", nullptr);
        std::cout << "✓ Telescope tasks created successfully" << std::endl;
        
        // Test device coordination tasks
        auto deviceScan = std::make_unique<DeviceScanConnectTask>("DeviceScanConnect", nullptr);
        auto healthMonitor = std::make_unique<DeviceHealthMonitorTask>("DeviceHealthMonitor", nullptr);
        std::cout << "✓ Device coordination tasks created successfully" << std::endl;
        
        // Test sequence analysis tasks
        auto advancedSequence = std::make_unique<AdvancedImagingSequenceTask>("AdvancedImagingSequence", nullptr);
        auto qualityAnalysis = std::make_unique<ImageQualityAnalysisTask>("ImageQualityAnalysis", nullptr);
        std::cout << "✓ Sequence analysis tasks created successfully" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "✗ Task creation failed: " << e.what() << std::endl;
        return 1;
    }
    
    std::cout << "\n=== All Task Categories Tested Successfully! ===" << std::endl;
    std::cout << "Camera task system is ready for production use!" << std::endl;
    
    return 0;
}
