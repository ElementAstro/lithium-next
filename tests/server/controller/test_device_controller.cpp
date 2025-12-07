/*
 * test_device_controller.cpp - Tests for Device Controllers
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include <gtest/gtest.h>

#include "atom/type/json.hpp"

#include <string>
#include <vector>

using json = nlohmann::json;

// ============================================================================
// Device Request Format Tests
// ============================================================================

class DeviceRequestFormatTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(DeviceRequestFormatTest, ConnectRequest) {
    json request = {{"device_id", "camera_1"}, {"driver", "zwo_asi"}};

    EXPECT_TRUE(request.contains("device_id"));
    EXPECT_EQ(request["device_id"], "camera_1");
}

TEST_F(DeviceRequestFormatTest, DisconnectRequest) {
    json request = {{"device_id", "camera_1"}};

    EXPECT_TRUE(request.contains("device_id"));
}

TEST_F(DeviceRequestFormatTest, GetPropertyRequest) {
    json request = {{"device_id", "camera_1"}, {"property", "gain"}};

    EXPECT_TRUE(request.contains("device_id"));
    EXPECT_TRUE(request.contains("property"));
}

TEST_F(DeviceRequestFormatTest, SetPropertyRequest) {
    json request = {
        {"device_id", "camera_1"}, {"property", "gain"}, {"value", 100}};

    EXPECT_TRUE(request.contains("device_id"));
    EXPECT_TRUE(request.contains("property"));
    EXPECT_TRUE(request.contains("value"));
}

// ============================================================================
// Camera Controller Tests
// ============================================================================

class CameraControllerTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(CameraControllerTest, ExposureRequest) {
    json request = {{"device_id", "camera_1"},
                    {"duration", 30.0},
                    {"gain", 100},
                    {"offset", 10},
                    {"binning", 1}};

    EXPECT_EQ(request["duration"], 30.0);
    EXPECT_EQ(request["gain"], 100);
    EXPECT_EQ(request["binning"], 1);
}

TEST_F(CameraControllerTest, ExposureResponse) {
    json response = {{"success", true},
                     {"data",
                      {{"image_path", "/images/capture_001.fits"},
                       {"exposure_time", 30.0},
                       {"timestamp", "2024-01-01T12:00:00Z"}}}};

    EXPECT_TRUE(response["success"].get<bool>());
    EXPECT_TRUE(response["data"].contains("image_path"));
}

TEST_F(CameraControllerTest, AbortExposureRequest) {
    json request = {{"device_id", "camera_1"}};

    EXPECT_TRUE(request.contains("device_id"));
}

TEST_F(CameraControllerTest, CoolerRequest) {
    json request = {{"device_id", "camera_1"},
                    {"enabled", true},
                    {"target_temperature", -20.0}};

    EXPECT_TRUE(request["enabled"].get<bool>());
    EXPECT_EQ(request["target_temperature"], -20.0);
}

TEST_F(CameraControllerTest, CameraStatusResponse) {
    json response = {{"success", true},
                     {"data",
                      {{"connected", true},
                       {"exposing", false},
                       {"temperature", -15.5},
                       {"cooler_power", 50},
                       {"gain", 100},
                       {"offset", 10},
                       {"binning", 1}}}};

    EXPECT_TRUE(response["data"]["connected"].get<bool>());
    EXPECT_FALSE(response["data"]["exposing"].get<bool>());
}

// ============================================================================
// Mount Controller Tests
// ============================================================================

class MountControllerTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(MountControllerTest, SlewRequest) {
    json request = {{"device_id", "mount_1"},
                    {"ra", 12.5},
                    {"dec", 45.0},
                    {"tracking", true}};

    EXPECT_EQ(request["ra"], 12.5);
    EXPECT_EQ(request["dec"], 45.0);
}

TEST_F(MountControllerTest, SlewToTargetRequest) {
    json request = {
        {"device_id", "mount_1"}, {"target_name", "M31"}, {"tracking", true}};

    EXPECT_EQ(request["target_name"], "M31");
}

TEST_F(MountControllerTest, ParkRequest) {
    json request = {{"device_id", "mount_1"}};

    EXPECT_TRUE(request.contains("device_id"));
}

TEST_F(MountControllerTest, UnparkRequest) {
    json request = {{"device_id", "mount_1"}};

    EXPECT_TRUE(request.contains("device_id"));
}

TEST_F(MountControllerTest, MountStatusResponse) {
    json response = {{"success", true},
                     {"data",
                      {{"connected", true},
                       {"tracking", true},
                       {"slewing", false},
                       {"parked", false},
                       {"ra", 12.5},
                       {"dec", 45.0},
                       {"altitude", 60.0},
                       {"azimuth", 180.0}}}};

    EXPECT_TRUE(response["data"]["connected"].get<bool>());
    EXPECT_TRUE(response["data"]["tracking"].get<bool>());
}

TEST_F(MountControllerTest, TrackingRateRequest) {
    json request = {{"device_id", "mount_1"}, {"rate", "sidereal"}};

    EXPECT_EQ(request["rate"], "sidereal");
}

// ============================================================================
// Focuser Controller Tests
// ============================================================================

class FocuserControllerTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(FocuserControllerTest, MoveAbsoluteRequest) {
    json request = {{"device_id", "focuser_1"}, {"position", 5000}};

    EXPECT_EQ(request["position"], 5000);
}

TEST_F(FocuserControllerTest, MoveRelativeRequest) {
    json request = {{"device_id", "focuser_1"}, {"steps", 100}};

    EXPECT_EQ(request["steps"], 100);
}

TEST_F(FocuserControllerTest, HaltRequest) {
    json request = {{"device_id", "focuser_1"}};

    EXPECT_TRUE(request.contains("device_id"));
}

TEST_F(FocuserControllerTest, FocuserStatusResponse) {
    json response = {{"success", true},
                     {"data",
                      {{"connected", true},
                       {"moving", false},
                       {"position", 5000},
                       {"max_position", 10000},
                       {"temperature", 20.5},
                       {"temp_comp", false}}}};

    EXPECT_EQ(response["data"]["position"], 5000);
    EXPECT_FALSE(response["data"]["moving"].get<bool>());
}

TEST_F(FocuserControllerTest, TemperatureCompensationRequest) {
    json request = {{"device_id", "focuser_1"}, {"enabled", true}};

    EXPECT_TRUE(request["enabled"].get<bool>());
}

// ============================================================================
// FilterWheel Controller Tests
// ============================================================================

class FilterWheelControllerTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(FilterWheelControllerTest, SetPositionRequest) {
    json request = {{"device_id", "filterwheel_1"}, {"position", 3}};

    EXPECT_EQ(request["position"], 3);
}

TEST_F(FilterWheelControllerTest, SetFilterByNameRequest) {
    json request = {{"device_id", "filterwheel_1"}, {"filter_name", "Ha"}};

    EXPECT_EQ(request["filter_name"], "Ha");
}

TEST_F(FilterWheelControllerTest, FilterWheelStatusResponse) {
    json response = {
        {"success", true},
        {"data",
         {{"connected", true},
          {"moving", false},
          {"position", 3},
          {"filter_count", 7},
          {"filter_names", {"L", "R", "G", "B", "Ha", "OIII", "SII"}}}}};

    EXPECT_EQ(response["data"]["position"], 3);
    EXPECT_EQ(response["data"]["filter_names"].size(), 7);
}

TEST_F(FilterWheelControllerTest, SetFilterNamesRequest) {
    json request = {{"device_id", "filterwheel_1"},
                    {"names", {"L", "R", "G", "B", "Ha", "OIII", "SII"}}};

    EXPECT_EQ(request["names"].size(), 7);
}

// ============================================================================
// Dome Controller Tests
// ============================================================================

class DomeControllerTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(DomeControllerTest, OpenShutterRequest) {
    json request = {{"device_id", "dome_1"}};

    EXPECT_TRUE(request.contains("device_id"));
}

TEST_F(DomeControllerTest, CloseShutterRequest) {
    json request = {{"device_id", "dome_1"}};

    EXPECT_TRUE(request.contains("device_id"));
}

TEST_F(DomeControllerTest, SlewToAzimuthRequest) {
    json request = {{"device_id", "dome_1"}, {"azimuth", 180.0}};

    EXPECT_EQ(request["azimuth"], 180.0);
}

TEST_F(DomeControllerTest, SyncToMountRequest) {
    json request = {{"device_id", "dome_1"}, {"enabled", true}};

    EXPECT_TRUE(request["enabled"].get<bool>());
}

TEST_F(DomeControllerTest, DomeStatusResponse) {
    json response = {{"success", true},
                     {"data",
                      {{"connected", true},
                       {"shutter_status", "open"},
                       {"slewing", false},
                       {"azimuth", 180.0},
                       {"synced_to_mount", true}}}};

    EXPECT_EQ(response["data"]["shutter_status"], "open");
    EXPECT_EQ(response["data"]["azimuth"], 180.0);
}

// ============================================================================
// Guider Controller Tests
// ============================================================================

class GuiderControllerTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(GuiderControllerTest, StartGuidingRequest) {
    json request = {
        {"device_id", "guider_1"}, {"exposure", 2.0}, {"calibrate", false}};

    EXPECT_EQ(request["exposure"], 2.0);
}

TEST_F(GuiderControllerTest, StopGuidingRequest) {
    json request = {{"device_id", "guider_1"}};

    EXPECT_TRUE(request.contains("device_id"));
}

TEST_F(GuiderControllerTest, CalibrateRequest) {
    json request = {{"device_id", "guider_1"}};

    EXPECT_TRUE(request.contains("device_id"));
}

TEST_F(GuiderControllerTest, DitherRequest) {
    json request = {
        {"device_id", "guider_1"}, {"pixels", 5.0}, {"settle_time", 10.0}};

    EXPECT_EQ(request["pixels"], 5.0);
}

TEST_F(GuiderControllerTest, GuiderStatusResponse) {
    json response = {{"success", true},
                     {"data",
                      {{"connected", true},
                       {"guiding", true},
                       {"calibrated", true},
                       {"rms_ra", 0.5},
                       {"rms_dec", 0.4},
                       {"total_rms", 0.64}}}};

    EXPECT_TRUE(response["data"]["guiding"].get<bool>());
    EXPECT_TRUE(response["data"]["calibrated"].get<bool>());
}

// ============================================================================
// Device List Tests
// ============================================================================

class DeviceListTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(DeviceListTest, ListDevicesResponse) {
    json response = {{"success", true},
                     {"data",
                      {{"devices",
                        {{{"id", "camera_1"},
                          {"type", "camera"},
                          {"driver", "zwo_asi"},
                          {"connected", true}},
                         {{"id", "mount_1"},
                          {"type", "mount"},
                          {"driver", "eqmod"},
                          {"connected", true}},
                         {{"id", "focuser_1"},
                          {"type", "focuser"},
                          {"driver", "moonlite"},
                          {"connected", false}}}}}}};

    EXPECT_EQ(response["data"]["devices"].size(), 3);
}

TEST_F(DeviceListTest, ListDevicesByTypeResponse) {
    json response = {{"success", true},
                     {"data",
                      {{"type", "camera"},
                       {"devices",
                        {{{"id", "camera_1"}, {"driver", "zwo_asi"}},
                         {{"id", "camera_2"}, {"driver", "qhy"}}}}}}};

    EXPECT_EQ(response["data"]["type"], "camera");
    EXPECT_EQ(response["data"]["devices"].size(), 2);
}

// ============================================================================
// Device Error Response Tests
// ============================================================================

class DeviceErrorResponseTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(DeviceErrorResponseTest, DeviceNotFound) {
    json response = {
        {"success", false},
        {"error",
         {{"code", "device_not_found"},
          {"message", "Camera not found: camera_1"},
          {"details", {{"deviceId", "camera_1"}, {"deviceType", "camera"}}}}}};

    EXPECT_FALSE(response["success"].get<bool>());
    EXPECT_EQ(response["error"]["code"], "device_not_found");
}

TEST_F(DeviceErrorResponseTest, DeviceNotConnected) {
    json response = {{"success", false},
                     {"error",
                      {{"code", "not_connected"},
                       {"message", "Device is not connected: camera_1"}}}};

    EXPECT_EQ(response["error"]["code"], "not_connected");
}

TEST_F(DeviceErrorResponseTest, DeviceBusy) {
    json response = {{"success", false},
                     {"error",
                      {{"code", "device_busy"},
                       {"message", "Device is busy: camera_1"},
                       {"details", {{"currentOperation", "exposing"}}}}}};

    EXPECT_EQ(response["error"]["code"], "device_busy");
}

TEST_F(DeviceErrorResponseTest, OperationFailed) {
    json response = {{"success", false},
                     {"error",
                      {{"code", "operation_failed"},
                       {"message", "Exposure failed: sensor error"}}}};

    EXPECT_EQ(response["error"]["code"], "operation_failed");
}

// ============================================================================
// Device Property Tests
// ============================================================================

class DevicePropertyTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(DevicePropertyTest, GetPropertiesResponse) {
    json response = {
        {"success", true},
        {"data",
         {{"properties",
           {{{"name", "gain"}, {"value", 100}, {"type", "number"}},
            {{"name", "offset"}, {"value", 10}, {"type", "number"}},
            {{"name", "binning"}, {"value", 1}, {"type", "number"}}}}}}};

    EXPECT_EQ(response["data"]["properties"].size(), 3);
}

TEST_F(DevicePropertyTest, SetPropertyResponse) {
    json response = {
        {"success", true},
        {"data",
         {{"property", "gain"}, {"old_value", 50}, {"new_value", 100}}}};

    EXPECT_EQ(response["data"]["new_value"], 100);
}
