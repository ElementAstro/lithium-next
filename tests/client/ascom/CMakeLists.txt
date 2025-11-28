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
