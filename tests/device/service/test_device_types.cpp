// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Lithium-Next - A modern astrophotography terminal
 * Copyright (C) 2024 Max Qian
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

/*
 * test_device_types.cpp
 *
 * Tests for device type definitions and utilities
 * - DeviceType enum conversions
 * - INDI interface mapping
 * - ASCOM device type mapping
 * - DeviceCapabilities
 */

#include <gtest/gtest.h>

#include "device/service/device_types.hpp"

using namespace lithium::device;

// ==================== DeviceType String Conversion Tests ====================

class DeviceTypeConversionTest : public ::testing::Test {};

TEST_F(DeviceTypeConversionTest, DeviceTypeToString_Camera) {
    EXPECT_EQ(deviceTypeToString(DeviceType::Camera), "Camera");
}

TEST_F(DeviceTypeConversionTest, DeviceTypeToString_Telescope) {
    EXPECT_EQ(deviceTypeToString(DeviceType::Telescope), "Telescope");
}

TEST_F(DeviceTypeConversionTest, DeviceTypeToString_Focuser) {
    EXPECT_EQ(deviceTypeToString(DeviceType::Focuser), "Focuser");
}

TEST_F(DeviceTypeConversionTest, DeviceTypeToString_FilterWheel) {
    EXPECT_EQ(deviceTypeToString(DeviceType::FilterWheel), "FilterWheel");
}

TEST_F(DeviceTypeConversionTest, DeviceTypeToString_Dome) {
    EXPECT_EQ(deviceTypeToString(DeviceType::Dome), "Dome");
}

TEST_F(DeviceTypeConversionTest, DeviceTypeToString_Rotator) {
    EXPECT_EQ(deviceTypeToString(DeviceType::Rotator), "Rotator");
}

TEST_F(DeviceTypeConversionTest, DeviceTypeToString_Weather) {
    EXPECT_EQ(deviceTypeToString(DeviceType::Weather), "Weather");
}

TEST_F(DeviceTypeConversionTest, DeviceTypeToString_GPS) {
    EXPECT_EQ(deviceTypeToString(DeviceType::GPS), "GPS");
}

TEST_F(DeviceTypeConversionTest, DeviceTypeToString_Guider) {
    EXPECT_EQ(deviceTypeToString(DeviceType::Guider), "Guider");
}

TEST_F(DeviceTypeConversionTest, DeviceTypeToString_Unknown) {
    EXPECT_EQ(deviceTypeToString(DeviceType::Unknown), "Unknown");
}

TEST_F(DeviceTypeConversionTest, StringToDeviceType_Camera) {
    EXPECT_EQ(stringToDeviceType("Camera"), DeviceType::Camera);
    EXPECT_EQ(stringToDeviceType("camera"), DeviceType::Camera);
    EXPECT_EQ(stringToDeviceType("CCD"), DeviceType::Camera);
    EXPECT_EQ(stringToDeviceType("ccd"), DeviceType::Camera);
}

TEST_F(DeviceTypeConversionTest, StringToDeviceType_Telescope) {
    EXPECT_EQ(stringToDeviceType("Telescope"), DeviceType::Telescope);
    EXPECT_EQ(stringToDeviceType("telescope"), DeviceType::Telescope);
    EXPECT_EQ(stringToDeviceType("Mount"), DeviceType::Telescope);
    EXPECT_EQ(stringToDeviceType("mount"), DeviceType::Telescope);
}

TEST_F(DeviceTypeConversionTest, StringToDeviceType_Focuser) {
    EXPECT_EQ(stringToDeviceType("Focuser"), DeviceType::Focuser);
    EXPECT_EQ(stringToDeviceType("focuser"), DeviceType::Focuser);
}

TEST_F(DeviceTypeConversionTest, StringToDeviceType_FilterWheel) {
    EXPECT_EQ(stringToDeviceType("FilterWheel"), DeviceType::FilterWheel);
    EXPECT_EQ(stringToDeviceType("filterwheel"), DeviceType::FilterWheel);
    EXPECT_EQ(stringToDeviceType("Filter Wheel"), DeviceType::FilterWheel);
}

TEST_F(DeviceTypeConversionTest, StringToDeviceType_Unknown) {
    EXPECT_EQ(stringToDeviceType("NonExistent"), DeviceType::Unknown);
    EXPECT_EQ(stringToDeviceType(""), DeviceType::Unknown);
    EXPECT_EQ(stringToDeviceType("invalid"), DeviceType::Unknown);
}

// ==================== GetAllDeviceTypes Tests ====================

class GetAllDeviceTypesTest : public ::testing::Test {};

TEST_F(GetAllDeviceTypesTest, ReturnsAllTypes) {
    auto types = getAllDeviceTypes();

    // Should contain all known device types (excluding Unknown)
    EXPECT_GE(types.size(), 14);

    // Check for specific types
    EXPECT_NE(std::find(types.begin(), types.end(), DeviceType::Camera),
              types.end());
    EXPECT_NE(std::find(types.begin(), types.end(), DeviceType::Telescope),
              types.end());
    EXPECT_NE(std::find(types.begin(), types.end(), DeviceType::Focuser),
              types.end());
    EXPECT_NE(std::find(types.begin(), types.end(), DeviceType::FilterWheel),
              types.end());
    EXPECT_NE(std::find(types.begin(), types.end(), DeviceType::Dome),
              types.end());
    EXPECT_NE(std::find(types.begin(), types.end(), DeviceType::Guider),
              types.end());
}

TEST_F(GetAllDeviceTypesTest, DoesNotContainUnknown) {
    auto types = getAllDeviceTypes();

    EXPECT_EQ(std::find(types.begin(), types.end(), DeviceType::Unknown),
              types.end());
}

// ==================== IsDeviceTypeSupported Tests ====================

class IsDeviceTypeSupportedTest : public ::testing::Test {};

TEST_F(IsDeviceTypeSupportedTest, SupportedTypes) {
    EXPECT_TRUE(isDeviceTypeSupported(DeviceType::Camera));
    EXPECT_TRUE(isDeviceTypeSupported(DeviceType::Telescope));
    EXPECT_TRUE(isDeviceTypeSupported(DeviceType::Focuser));
    EXPECT_TRUE(isDeviceTypeSupported(DeviceType::FilterWheel));
    EXPECT_TRUE(isDeviceTypeSupported(DeviceType::Dome));
    EXPECT_TRUE(isDeviceTypeSupported(DeviceType::Guider));
}

TEST_F(IsDeviceTypeSupportedTest, UnsupportedType) {
    EXPECT_FALSE(isDeviceTypeSupported(DeviceType::Unknown));
}

// ==================== INDI Interface Mapping Tests ====================

class INDIInterfaceMappingTest : public ::testing::Test {};

TEST_F(INDIInterfaceMappingTest, TelescopeInterface) {
    // INDI_TELESCOPE = 1 << 0 = 1
    auto types = indiInterfaceToDeviceTypes(1);
    EXPECT_EQ(types.size(), 1);
    EXPECT_EQ(types[0], DeviceType::Telescope);
}

TEST_F(INDIInterfaceMappingTest, CCDInterface) {
    // INDI_CCD = 1 << 1 = 2
    auto types = indiInterfaceToDeviceTypes(2);
    EXPECT_EQ(types.size(), 1);
    EXPECT_EQ(types[0], DeviceType::Camera);
}

TEST_F(INDIInterfaceMappingTest, FocuserInterface) {
    // INDI_FOCUSER = 1 << 3 = 8
    auto types = indiInterfaceToDeviceTypes(8);
    EXPECT_EQ(types.size(), 1);
    EXPECT_EQ(types[0], DeviceType::Focuser);
}

TEST_F(INDIInterfaceMappingTest, FilterInterface) {
    // INDI_FILTER = 1 << 4 = 16
    auto types = indiInterfaceToDeviceTypes(16);
    EXPECT_EQ(types.size(), 1);
    EXPECT_EQ(types[0], DeviceType::FilterWheel);
}

TEST_F(INDIInterfaceMappingTest, DomeInterface) {
    // INDI_DOME = 1 << 5 = 32
    auto types = indiInterfaceToDeviceTypes(32);
    EXPECT_EQ(types.size(), 1);
    EXPECT_EQ(types[0], DeviceType::Dome);
}

TEST_F(INDIInterfaceMappingTest, MultipleInterfaces) {
    // INDI_TELESCOPE | INDI_CCD | INDI_FOCUSER = 1 | 2 | 8 = 11
    auto types = indiInterfaceToDeviceTypes(11);
    EXPECT_EQ(types.size(), 3);

    EXPECT_NE(std::find(types.begin(), types.end(), DeviceType::Telescope),
              types.end());
    EXPECT_NE(std::find(types.begin(), types.end(), DeviceType::Camera),
              types.end());
    EXPECT_NE(std::find(types.begin(), types.end(), DeviceType::Focuser),
              types.end());
}

TEST_F(INDIInterfaceMappingTest, NoInterface) {
    auto types = indiInterfaceToDeviceTypes(0);
    EXPECT_TRUE(types.empty());
}

// ==================== ASCOM Device Type Mapping Tests ====================

class ASCOMDeviceTypeMappingTest : public ::testing::Test {};

TEST_F(ASCOMDeviceTypeMappingTest, Camera) {
    EXPECT_EQ(ascomDeviceTypeToDeviceType("Camera"), DeviceType::Camera);
}

TEST_F(ASCOMDeviceTypeMappingTest, Telescope) {
    EXPECT_EQ(ascomDeviceTypeToDeviceType("Telescope"), DeviceType::Telescope);
}

TEST_F(ASCOMDeviceTypeMappingTest, Focuser) {
    EXPECT_EQ(ascomDeviceTypeToDeviceType("Focuser"), DeviceType::Focuser);
}

TEST_F(ASCOMDeviceTypeMappingTest, FilterWheel) {
    EXPECT_EQ(ascomDeviceTypeToDeviceType("FilterWheel"),
              DeviceType::FilterWheel);
}

TEST_F(ASCOMDeviceTypeMappingTest, Dome) {
    EXPECT_EQ(ascomDeviceTypeToDeviceType("Dome"), DeviceType::Dome);
}

TEST_F(ASCOMDeviceTypeMappingTest, Rotator) {
    EXPECT_EQ(ascomDeviceTypeToDeviceType("Rotator"), DeviceType::Rotator);
}

TEST_F(ASCOMDeviceTypeMappingTest, SafetyMonitor) {
    EXPECT_EQ(ascomDeviceTypeToDeviceType("SafetyMonitor"),
              DeviceType::SafetyMonitor);
}

TEST_F(ASCOMDeviceTypeMappingTest, Switch) {
    EXPECT_EQ(ascomDeviceTypeToDeviceType("Switch"), DeviceType::Switch);
}

TEST_F(ASCOMDeviceTypeMappingTest, Unknown) {
    EXPECT_EQ(ascomDeviceTypeToDeviceType("NonExistent"), DeviceType::Unknown);
    EXPECT_EQ(ascomDeviceTypeToDeviceType(""), DeviceType::Unknown);
}

// ==================== DeviceCapabilities Tests ====================

class DeviceCapabilitiesTest : public ::testing::Test {};

TEST_F(DeviceCapabilitiesTest, DefaultCapabilities) {
    DeviceCapabilities caps;

    EXPECT_TRUE(caps.canConnect);
    EXPECT_TRUE(caps.canDisconnect);
    EXPECT_FALSE(caps.canAbort);
    EXPECT_FALSE(caps.canPark);
    EXPECT_FALSE(caps.canHome);
    EXPECT_FALSE(caps.canSync);
    EXPECT_FALSE(caps.canSlew);
    EXPECT_FALSE(caps.canTrack);
    EXPECT_FALSE(caps.canGuide);
    EXPECT_FALSE(caps.canCool);
    EXPECT_FALSE(caps.canFocus);
    EXPECT_FALSE(caps.canRotate);
    EXPECT_FALSE(caps.hasShutter);
    EXPECT_FALSE(caps.hasTemperature);
    EXPECT_FALSE(caps.hasPosition);
}

TEST_F(DeviceCapabilitiesTest, ToJson) {
    DeviceCapabilities caps;
    caps.canAbort = true;
    caps.canPark = true;
    caps.hasTemperature = true;

    auto json = caps.toJson();

    EXPECT_TRUE(json["canConnect"].get<bool>());
    EXPECT_TRUE(json["canDisconnect"].get<bool>());
    EXPECT_TRUE(json["canAbort"].get<bool>());
    EXPECT_TRUE(json["canPark"].get<bool>());
    EXPECT_FALSE(json["canHome"].get<bool>());
    EXPECT_TRUE(json["hasTemperature"].get<bool>());
}

// ==================== GetDefaultCapabilities Tests ====================

class GetDefaultCapabilitiesTest : public ::testing::Test {};

TEST_F(GetDefaultCapabilitiesTest, CameraCapabilities) {
    auto caps = getDefaultCapabilities(DeviceType::Camera);

    EXPECT_TRUE(caps.canAbort);
    EXPECT_TRUE(caps.canCool);
    EXPECT_TRUE(caps.hasTemperature);
    EXPECT_FALSE(caps.canPark);
    EXPECT_FALSE(caps.canSlew);
}

TEST_F(GetDefaultCapabilitiesTest, TelescopeCapabilities) {
    auto caps = getDefaultCapabilities(DeviceType::Telescope);

    EXPECT_TRUE(caps.canAbort);
    EXPECT_TRUE(caps.canPark);
    EXPECT_TRUE(caps.canHome);
    EXPECT_TRUE(caps.canSync);
    EXPECT_TRUE(caps.canSlew);
    EXPECT_TRUE(caps.canTrack);
    EXPECT_TRUE(caps.canGuide);
    EXPECT_TRUE(caps.hasPosition);
    EXPECT_FALSE(caps.canCool);
}

TEST_F(GetDefaultCapabilitiesTest, FocuserCapabilities) {
    auto caps = getDefaultCapabilities(DeviceType::Focuser);

    EXPECT_TRUE(caps.canAbort);
    EXPECT_TRUE(caps.canFocus);
    EXPECT_TRUE(caps.hasPosition);
    EXPECT_TRUE(caps.hasTemperature);
    EXPECT_FALSE(caps.canPark);
    EXPECT_FALSE(caps.canSlew);
}

TEST_F(GetDefaultCapabilitiesTest, FilterWheelCapabilities) {
    auto caps = getDefaultCapabilities(DeviceType::FilterWheel);

    EXPECT_TRUE(caps.hasPosition);
    EXPECT_FALSE(caps.canAbort);
    EXPECT_FALSE(caps.canPark);
}

TEST_F(GetDefaultCapabilitiesTest, DomeCapabilities) {
    auto caps = getDefaultCapabilities(DeviceType::Dome);

    EXPECT_TRUE(caps.canAbort);
    EXPECT_TRUE(caps.canPark);
    EXPECT_TRUE(caps.canHome);
    EXPECT_TRUE(caps.hasShutter);
    EXPECT_TRUE(caps.hasPosition);
    EXPECT_FALSE(caps.canSlew);
}

TEST_F(GetDefaultCapabilitiesTest, RotatorCapabilities) {
    auto caps = getDefaultCapabilities(DeviceType::Rotator);

    EXPECT_TRUE(caps.canAbort);
    EXPECT_TRUE(caps.canRotate);
    EXPECT_TRUE(caps.hasPosition);
    EXPECT_FALSE(caps.canPark);
}

TEST_F(GetDefaultCapabilitiesTest, GuiderCapabilities) {
    auto caps = getDefaultCapabilities(DeviceType::Guider);

    EXPECT_TRUE(caps.canGuide);
    EXPECT_FALSE(caps.canAbort);
    EXPECT_FALSE(caps.canPark);
}

TEST_F(GetDefaultCapabilitiesTest, UnknownCapabilities) {
    auto caps = getDefaultCapabilities(DeviceType::Unknown);

    // Unknown type should have default capabilities
    EXPECT_TRUE(caps.canConnect);
    EXPECT_TRUE(caps.canDisconnect);
    EXPECT_FALSE(caps.canAbort);
}

// ==================== Main ====================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
