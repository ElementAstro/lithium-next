# ============================================================================
# CMakeLists.txt for ASCOM Client Tests
# ============================================================================

cmake_minimum_required(VERSION 3.20)

# Test for ASCOM client
lithium_add_test(
    NAME test_ascom_client
    SOURCES test_ascom_client.cpp
    LIBS lithium.client.ascom lithium.client.common atom
)

# Test for ASCOM device base
lithium_add_test(
    NAME test_ascom_device_base
    SOURCES test_ascom_device_base.cpp
    LIBS lithium.client.ascom lithium.client.common atom
)

# Test for ASCOM camera
lithium_add_test(
    NAME test_ascom_camera
    SOURCES test_ascom_camera.cpp
    LIBS lithium.client.ascom lithium.client.common atom
)

# Test for ASCOM device factory
lithium_add_test(
    NAME test_ascom_device_factory
    SOURCES test_ascom_device_factory.cpp
    LIBS lithium.client.ascom lithium.client.common atom
)
