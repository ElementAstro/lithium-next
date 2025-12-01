# ============================================================================
# CMakeLists.txt for Device Service Tests
# ============================================================================

cmake_minimum_required(VERSION 3.20)

# Test for INDI adapter
lithium_add_test(
    NAME test_indi_adapter
    SOURCES test_indi_adapter.cpp
    LIBS lithium_device_service lithium.client.indi atom
)

# Test for ASCOM adapter
lithium_add_test(
    NAME test_ascom_adapter
    SOURCES test_ascom_adapter.cpp
    LIBS lithium_device_service lithium.client.ascom atom
)

# Test for Guider service
lithium_add_test(
    NAME test_guider_service
    SOURCES test_guider_service.cpp
    LIBS lithium_device_service lithium_phd2_guider atom
)

# Test for Base service
lithium_add_test(
    NAME test_base_service
    SOURCES test_base_service.cpp
    LIBS lithium_device_service lithium_device atom
)

# Test for Device Backend
lithium_add_test(
    NAME test_device_backend
    SOURCES test_device_backend.cpp
    LIBS lithium_device_service lithium.client.indi lithium.client.ascom atom
)
