# ============================================================================
# CMakeLists.txt for INDI Client Tests
# ============================================================================

cmake_minimum_required(VERSION 3.20)

# Test for INDI client
lithium_add_test(
    NAME test_indi_client
    SOURCES test_indi_client.cpp
    LIBS lithium.client.indi lithium.client.common atom
)
