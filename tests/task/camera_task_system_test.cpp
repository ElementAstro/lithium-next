#include <gtest/gtest.h>
#include <memory>
#include "task/custom/camera/camera_tasks.hpp"
#include "task/custom/factory.hpp"

namespace lithium::task::test {

/**
 * @brief Test suite for the optimized camera task system
 * 
 * This test suite validates all the new camera tasks to ensure they:
 * 1. Register correctly with the factory
 * 2. Execute without errors for valid parameters
 * 3. Properly validate parameters
 * 4. Handle error conditions gracefully
 */
class CameraTaskSystemTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Factory should be automatically populated by static registration
        factory_ = &TaskFactory::getInstance();
    }

    TaskFactory* factory_;
};

// ==================== Video Task Tests ====================

TEST_F(CameraTaskSystemTest, VideoTasksRegistered) {
    EXPECT_TRUE(factory_->isTaskRegistered("StartVideo"));
    EXPECT_TRUE(factory_->isTaskRegistered("StopVideo"));
    EXPECT_TRUE(factory_->isTaskRegistered("GetVideoFrame"));
    EXPECT_TRUE(factory_->isTaskRegistered("RecordVideo"));
    EXPECT_TRUE(factory_->isTaskRegistered("VideoStreamMonitor"));
}

TEST_F(CameraTaskSystemTest, StartVideoTaskExecution) {
    auto task = factory_->createTask("StartVideo", "test_start_video", json{});
    ASSERT_NE(task, nullptr);
    
    json params = {
        {"stabilize_delay", 1000},
        {"format", "RGB24"},
        {"fps", 30.0}
    };
    
    EXPECT_NO_THROW(task->execute(params));
    EXPECT_EQ(task->getStatus(), TaskStatus::Completed);
}

TEST_F(CameraTaskSystemTest, RecordVideoTaskValidation) {
    auto task = factory_->createTask("RecordVideo", "test_record_video", json{});
    ASSERT_NE(task, nullptr);
    
    // Test invalid duration
    json invalidParams = {{"duration", 0}};
    EXPECT_THROW(task->execute(invalidParams), std::exception);
    
    // Test valid parameters
    json validParams = {
        {"duration", 10},
        {"filename", "test_video.mp4"},
        {"quality", "high"},
        {"fps", 30.0}
    };
    EXPECT_NO_THROW(task->execute(validParams));
}

// ==================== Temperature Task Tests ====================

TEST_F(CameraTaskSystemTest, TemperatureTasksRegistered) {
    EXPECT_TRUE(factory_->isTaskRegistered("CoolingControl"));
    EXPECT_TRUE(factory_->isTaskRegistered("TemperatureMonitor"));
    EXPECT_TRUE(factory_->isTaskRegistered("TemperatureStabilization"));
    EXPECT_TRUE(factory_->isTaskRegistered("CoolingOptimization"));
    EXPECT_TRUE(factory_->isTaskRegistered("TemperatureAlert"));
}

TEST_F(CameraTaskSystemTest, CoolingControlTaskExecution) {
    auto task = factory_->createTask("CoolingControl", "test_cooling", json{});
    ASSERT_NE(task, nullptr);
    
    json params = {
        {"enable", true},
        {"target_temperature", -15.0},
        {"wait_for_stabilization", false}
    };
    
    EXPECT_NO_THROW(task->execute(params));
    EXPECT_EQ(task->getStatus(), TaskStatus::Completed);
}

TEST_F(CameraTaskSystemTest, TemperatureStabilizationValidation) {
    auto task = factory_->createTask("TemperatureStabilization", "test_stabilization", json{});
    ASSERT_NE(task, nullptr);
    
    // Test missing required parameter
    json invalidParams = {{"tolerance", 1.0}};
    EXPECT_THROW(task->execute(invalidParams), std::exception);
    
    // Test valid parameters
    json validParams = {
        {"target_temperature", -20.0},
        {"tolerance", 1.0},
        {"max_wait_time", 300}
    };
    EXPECT_NO_THROW(task->execute(validParams));
}

// ==================== Frame Task Tests ====================

TEST_F(CameraTaskSystemTest, FrameTasksRegistered) {
    EXPECT_TRUE(factory_->isTaskRegistered("FrameConfig"));
    EXPECT_TRUE(factory_->isTaskRegistered("ROIConfig"));
    EXPECT_TRUE(factory_->isTaskRegistered("BinningConfig"));
    EXPECT_TRUE(factory_->isTaskRegistered("FrameInfo"));
    EXPECT_TRUE(factory_->isTaskRegistered("UploadMode"));
    EXPECT_TRUE(factory_->isTaskRegistered("FrameStats"));
}

TEST_F(CameraTaskSystemTest, FrameConfigTaskExecution) {
    auto task = factory_->createTask("FrameConfig", "test_frame_config", json{});
    ASSERT_NE(task, nullptr);
    
    json params = {
        {"width", 1920},
        {"height", 1080},
        {"binning", {{"x", 1}, {"y", 1}}},
        {"frame_type", "FITS"},
        {"upload_mode", "LOCAL"}
    };
    
    EXPECT_NO_THROW(task->execute(params));
    EXPECT_EQ(task->getStatus(), TaskStatus::Completed);
}

TEST_F(CameraTaskSystemTest, ROIConfigValidation) {
    auto task = factory_->createTask("ROIConfig", "test_roi", json{});
    ASSERT_NE(task, nullptr);
    
    // Test invalid ROI (exceeds sensor bounds)
    json invalidParams = {
        {"x", 0},
        {"y", 0},
        {"width", 10000},
        {"height", 10000}
    };
    EXPECT_THROW(task->execute(invalidParams), std::exception);
    
    // Test valid ROI
    json validParams = {
        {"x", 100},
        {"y", 100},
        {"width", 1000},
        {"height", 1000}
    };
    EXPECT_NO_THROW(task->execute(validParams));
}

// ==================== Parameter Task Tests ====================

TEST_F(CameraTaskSystemTest, ParameterTasksRegistered) {
    EXPECT_TRUE(factory_->isTaskRegistered("GainControl"));
    EXPECT_TRUE(factory_->isTaskRegistered("OffsetControl"));
    EXPECT_TRUE(factory_->isTaskRegistered("ISOControl"));
    EXPECT_TRUE(factory_->isTaskRegistered("AutoParameter"));
    EXPECT_TRUE(factory_->isTaskRegistered("ParameterProfile"));
    EXPECT_TRUE(factory_->isTaskRegistered("ParameterStatus"));
}

TEST_F(CameraTaskSystemTest, GainControlTaskExecution) {
    auto task = factory_->createTask("GainControl", "test_gain", json{});
    ASSERT_NE(task, nullptr);
    
    json params = {
        {"gain", 200},
        {"mode", "manual"}
    };
    
    EXPECT_NO_THROW(task->execute(params));
    EXPECT_EQ(task->getStatus(), TaskStatus::Completed);
}

TEST_F(CameraTaskSystemTest, ISOControlValidation) {
    auto task = factory_->createTask("ISOControl", "test_iso", json{});
    ASSERT_NE(task, nullptr);
    
    // Test invalid ISO
    json invalidParams = {{"iso", 999}};
    EXPECT_THROW(task->execute(invalidParams), std::exception);
    
    // Test valid ISO
    json validParams = {{"iso", 800}};
    EXPECT_NO_THROW(task->execute(validParams));
}

TEST_F(CameraTaskSystemTest, ParameterProfileManagement) {
    auto saveTask = factory_->createTask("ParameterProfile", "test_save_profile", json{});
    auto loadTask = factory_->createTask("ParameterProfile", "test_load_profile", json{});
    auto listTask = factory_->createTask("ParameterProfile", "test_list_profiles", json{});
    
    ASSERT_NE(saveTask, nullptr);
    ASSERT_NE(loadTask, nullptr);
    ASSERT_NE(listTask, nullptr);
    
    // Save a profile
    json saveParams = {
        {"action", "save"},
        {"name", "test_profile"}
    };
    EXPECT_NO_THROW(saveTask->execute(saveParams));
    
    // List profiles
    json listParams = {{"action", "list"}};
    EXPECT_NO_THROW(listTask->execute(listParams));
    
    // Load the profile
    json loadParams = {
        {"action", "load"},
        {"name", "test_profile"}
    };
    EXPECT_NO_THROW(loadTask->execute(loadParams));
}

// ==================== Integration Tests ====================

TEST_F(CameraTaskSystemTest, TaskDependencies) {
    // Test that dependent tasks can be executed in sequence
    
    // 1. Start cooling
    auto coolingTask = factory_->createTask("CoolingControl", "test_cooling_seq", json{});
    json coolingParams = {
        {"enable", true},
        {"target_temperature", -10.0}
    };
    EXPECT_NO_THROW(coolingTask->execute(coolingParams));
    
    // 2. Wait for stabilization (depends on cooling)
    auto stabilizationTask = factory_->createTask("TemperatureStabilization", "test_stabilization_seq", json{});
    json stabilizationParams = {
        {"target_temperature", -10.0},
        {"tolerance", 2.0},
        {"max_wait_time", 60}
    };
    EXPECT_NO_THROW(stabilizationTask->execute(stabilizationParams));
    
    // 3. Configure frame settings
    auto frameTask = factory_->createTask("FrameConfig", "test_frame_seq", json{});
    json frameParams = {
        {"width", 2048},
        {"height", 2048},
        {"frame_type", "FITS"}
    };
    EXPECT_NO_THROW(frameTask->execute(frameParams));
}

TEST_F(CameraTaskSystemTest, ErrorHandling) {
    auto task = factory_->createTask("GainControl", "test_error_handling", json{});
    ASSERT_NE(task, nullptr);
    
    // Test error propagation
    json invalidParams = {{"gain", -100}};
    EXPECT_THROW(task->execute(invalidParams), std::exception);
    EXPECT_EQ(task->getStatus(), TaskStatus::Failed);
    EXPECT_EQ(task->getErrorType(), TaskErrorType::InvalidParameter);
}

TEST_F(CameraTaskSystemTest, TaskInfoValidation) {
    // Verify task info is properly set for all new tasks
    std::vector<std::string> newTasks = {
        "StartVideo", "StopVideo", "GetVideoFrame", "RecordVideo", "VideoStreamMonitor",
        "CoolingControl", "TemperatureMonitor", "TemperatureStabilization", 
        "CoolingOptimization", "TemperatureAlert",
        "FrameConfig", "ROIConfig", "BinningConfig", "FrameInfo", "UploadMode", "FrameStats",
        "GainControl", "OffsetControl", "ISOControl", "AutoParameter", 
        "ParameterProfile", "ParameterStatus"
    };
    
    for (const auto& taskName : newTasks) {
        EXPECT_TRUE(factory_->isTaskRegistered(taskName)) << "Task " << taskName << " not registered";
        
        auto info = factory_->getTaskInfo(taskName);
        EXPECT_FALSE(info.name.empty()) << "Task " << taskName << " has empty name";
        EXPECT_FALSE(info.description.empty()) << "Task " << taskName << " has empty description";
        EXPECT_FALSE(info.category.empty()) << "Task " << taskName << " has empty category";
        EXPECT_FALSE(info.version.empty()) << "Task " << taskName << " has empty version";
    }
}

}  // namespace lithium::task::test
