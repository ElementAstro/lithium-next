/*
 * test_array_ops.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file test_array_ops.cpp
 * @brief Comprehensive tests for NumPy Array Operations
 */

#include <gtest/gtest.h>
#include "script/numpy/array_ops.hpp"

#include <vector>

using namespace lithium::numpy;

// =============================================================================
// Test Fixture
// =============================================================================

class ArrayOpsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize Python if needed
        try {
            py::initialize_interpreter();
        } catch (...) {
            // Already initialized
        }
    }
};

// =============================================================================
// Array Creation Tests
// =============================================================================

TEST_F(ArrayOpsTest, CreateArrayFromVector) {
    std::vector<float> data = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    auto arr = ArrayOps::createArray(data);

    EXPECT_EQ(arr.size(), 5);
}

TEST_F(ArrayOpsTest, CreateArrayFromVectorDouble) {
    std::vector<double> data = {1.0, 2.0, 3.0};
    auto arr = ArrayOps::createArray(data);

    EXPECT_EQ(arr.size(), 3);
}

TEST_F(ArrayOpsTest, CreateArrayFromVectorInt) {
    std::vector<int32_t> data = {1, 2, 3, 4};
    auto arr = ArrayOps::createArray(data);

    EXPECT_EQ(arr.size(), 4);
}

TEST_F(ArrayOpsTest, CreateArray2D) {
    std::vector<std::vector<float>> data = {{1.0f, 2.0f, 3.0f},
                                            {4.0f, 5.0f, 6.0f}};
    auto arr = ArrayOps::createArray2D(data);

    auto shape = ArrayOps::getShape(arr);
    EXPECT_EQ(shape.size(), 2);
    EXPECT_EQ(shape[0], 2);
    EXPECT_EQ(shape[1], 3);
}

TEST_F(ArrayOpsTest, CreateZerosArray) {
    std::vector<size_t> shape = {3, 4};
    auto arr = ArrayOps::zeros<float>(shape);

    EXPECT_EQ(arr.size(), 12);
}

TEST_F(ArrayOpsTest, CreateEmptyArray) {
    std::vector<size_t> shape = {2, 3};
    auto arr = ArrayOps::empty<double>(shape);

    EXPECT_EQ(arr.size(), 6);
}

TEST_F(ArrayOpsTest, CreateFullArray) {
    std::vector<size_t> shape = {2, 2};
    auto arr = ArrayOps::full<int32_t>(shape, 42);

    auto vec = ArrayOps::toVector(arr);
    for (auto val : vec) {
        EXPECT_EQ(val, 42);
    }
}

// =============================================================================
// Array Conversion Tests
// =============================================================================

TEST_F(ArrayOpsTest, ToVector) {
    std::vector<float> original = {1.0f, 2.0f, 3.0f};
    auto arr = ArrayOps::createArray(original);
    auto result = ArrayOps::toVector(arr);

    EXPECT_EQ(result, original);
}

TEST_F(ArrayOpsTest, ToVector2D) {
    std::vector<std::vector<double>> original = {{1.0, 2.0}, {3.0, 4.0}};
    auto arr = ArrayOps::createArray2D(original);
    auto result = ArrayOps::toVector2D(arr);

    EXPECT_EQ(result, original);
}

TEST_F(ArrayOpsTest, CopyToBuffer) {
    std::vector<float> data = {1.0f, 2.0f, 3.0f, 4.0f};
    auto arr = ArrayOps::createArray(data);

    float buffer[4];
    ArrayOps::copyToBuffer(arr, buffer, 4);

    for (size_t i = 0; i < 4; ++i) {
        EXPECT_FLOAT_EQ(buffer[i], data[i]);
    }
}

// =============================================================================
// Shape and Dtype Tests
// =============================================================================

TEST_F(ArrayOpsTest, GetShape1D) {
    std::vector<float> data = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    auto arr = ArrayOps::createArray(data);
    auto shape = ArrayOps::getShape(arr);

    EXPECT_EQ(shape.size(), 1);
    EXPECT_EQ(shape[0], 5);
}

TEST_F(ArrayOpsTest, GetShape2D) {
    std::vector<std::vector<float>> data = {{1.0f, 2.0f, 3.0f},
                                            {4.0f, 5.0f, 6.0f}};
    auto arr = ArrayOps::createArray2D(data);
    auto shape = ArrayOps::getShape(arr);

    EXPECT_EQ(shape.size(), 2);
    EXPECT_EQ(shape[0], 2);
    EXPECT_EQ(shape[1], 3);
}

TEST_F(ArrayOpsTest, GetDtypeNameFloat) {
    std::vector<float> data = {1.0f};
    auto arr = ArrayOps::createArray(data);
    auto dtype = ArrayOps::getDtypeName(arr);

    EXPECT_EQ(dtype, "float32");
}

TEST_F(ArrayOpsTest, GetDtypeNameDouble) {
    std::vector<double> data = {1.0};
    auto arr = ArrayOps::createArray(data);
    auto dtype = ArrayOps::getDtypeName(arr);

    EXPECT_EQ(dtype, "float64");
}

// =============================================================================
// Array Operations Tests
// =============================================================================

TEST_F(ArrayOpsTest, ReshapeArray) {
    std::vector<float> data = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};
    auto arr = ArrayOps::createArray(data);

    auto result = ArrayOps::reshape(arr, {2, 3});
    ASSERT_TRUE(result.has_value());

    auto shape = ArrayOps::getShape(result.value());
    EXPECT_EQ(shape[0], 2);
    EXPECT_EQ(shape[1], 3);
}

TEST_F(ArrayOpsTest, ReshapeInvalidShape) {
    std::vector<float> data = {1.0f, 2.0f, 3.0f, 4.0f};
    auto arr = ArrayOps::createArray(data);

    auto result = ArrayOps::reshape(arr, {2, 3});  // 2*3 != 4
    EXPECT_FALSE(result.has_value());
}

TEST_F(ArrayOpsTest, TransposeArray) {
    std::vector<std::vector<float>> data = {{1.0f, 2.0f, 3.0f},
                                            {4.0f, 5.0f, 6.0f}};
    auto arr = ArrayOps::createArray2D(data);

    auto result = ArrayOps::transpose(arr);
    ASSERT_TRUE(result.has_value());

    auto shape = ArrayOps::getShape(result.value());
    EXPECT_EQ(shape[0], 3);
    EXPECT_EQ(shape[1], 2);
}

TEST_F(ArrayOpsTest, StackArrays) {
    std::vector<float> data1 = {1.0f, 2.0f};
    std::vector<float> data2 = {3.0f, 4.0f};

    auto arr1 = ArrayOps::createArray(data1);
    auto arr2 = ArrayOps::createArray(data2);

    std::vector<py::array> arrays = {arr1, arr2};
    auto result = ArrayOps::stack(arrays);

    ASSERT_TRUE(result.has_value());
    auto shape = ArrayOps::getShape(result.value());
    EXPECT_EQ(shape[0], 2);
    EXPECT_EQ(shape[1], 2);
}

TEST_F(ArrayOpsTest, ConcatenateArrays) {
    std::vector<float> data1 = {1.0f, 2.0f};
    std::vector<float> data2 = {3.0f, 4.0f};

    auto arr1 = ArrayOps::createArray(data1);
    auto arr2 = ArrayOps::createArray(data2);

    std::vector<py::array> arrays = {arr1, arr2};
    auto result = ArrayOps::concatenate(arrays);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->size(), 4);
}

// =============================================================================
// NumpyDtype Tests
// =============================================================================

TEST_F(ArrayOpsTest, NumpyDtypeFloat) {
    EXPECT_STREQ(NumpyDtype<float>::format, "f");
    EXPECT_STREQ(NumpyDtype<float>::name, "float32");
}

TEST_F(ArrayOpsTest, NumpyDtypeDouble) {
    EXPECT_STREQ(NumpyDtype<double>::format, "d");
    EXPECT_STREQ(NumpyDtype<double>::name, "float64");
}

TEST_F(ArrayOpsTest, NumpyDtypeInt32) {
    EXPECT_STREQ(NumpyDtype<int32_t>::format, "i");
    EXPECT_STREQ(NumpyDtype<int32_t>::name, "int32");
}

TEST_F(ArrayOpsTest, NumpyDtypeUint8) {
    EXPECT_STREQ(NumpyDtype<uint8_t>::format, "B");
    EXPECT_STREQ(NumpyDtype<uint8_t>::name, "uint8");
}

TEST_F(ArrayOpsTest, NumpyDtypeBool) {
    EXPECT_STREQ(NumpyDtype<bool>::format, "?");
    EXPECT_STREQ(NumpyDtype<bool>::name, "bool");
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST_F(ArrayOpsTest, CreateEmptyVectorArray) {
    std::vector<float> empty;
    auto arr = ArrayOps::createArray(empty);
    EXPECT_EQ(arr.size(), 0);
}

TEST_F(ArrayOpsTest, CreateEmpty2DArray) {
    std::vector<std::vector<float>> empty;
    auto arr = ArrayOps::createArray2D(empty);

    auto shape = ArrayOps::getShape(arr);
    EXPECT_EQ(shape[0], 0);
}

TEST_F(ArrayOpsTest, LargeArray) {
    std::vector<float> large(1000000, 1.0f);
    auto arr = ArrayOps::createArray(large);
    EXPECT_EQ(arr.size(), 1000000);
}
