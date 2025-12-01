// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Lithium-Next - A modern astrophotography terminal
 * Copyright (C) 2024 Max Qian
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include <gtest/gtest.h>

#include <filesystem>
#include <sstream>

#include "target/io/csv_handler.hpp"

namespace fs = std::filesystem;

class CsvHandlerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create temporary directory for test files
        test_dir = fs::temp_directory_path() / "lithium_csv_test";
        fs::create_directories(test_dir);
    }

    void TearDown() override {
        // Clean up test files
        fs::remove_all(test_dir);
    }

    fs::path test_dir;
};

TEST_F(CsvHandlerTest, ParseSimpleCSV) {
    lithium::target::io::CsvHandler handler;

    // Create a simple CSV file
    std::string csv_content =
        "name,value\n"
        "field1,value1\n"
        "field2,value2\n";

    auto csv_file = test_dir / "simple.csv";
    std::ofstream out(csv_file);
    out << csv_content;
    out.close();

    // Read the CSV file
    auto result = handler.read(csv_file.string());
    EXPECT_TRUE(result);

    auto records = result.value();
    EXPECT_EQ(records.size(), 2);

    // Check first record
    EXPECT_EQ(records[0]["name"], "field1");
    EXPECT_EQ(records[0]["value"], "value1");

    // Check second record
    EXPECT_EQ(records[1]["name"], "field2");
    EXPECT_EQ(records[1]["value"], "value2");
}

TEST_F(CsvHandlerTest, WriteCSV) {
    lithium::target::io::CsvHandler handler;

    // Prepare data
    std::vector<std::unordered_map<std::string, std::string>> data = {
        {{"name", "field1"}, {"value", "value1"}},
        {{"name", "field2"}, {"value", "value2"}},
    };

    std::vector<std::string> fields = {"name", "value"};
    auto csv_file = test_dir / "output.csv";

    // Write CSV file
    auto result = handler.write(csv_file.string(), data, fields);
    EXPECT_TRUE(result);
    EXPECT_EQ(result.value(), 2);

    // Verify file was created
    EXPECT_TRUE(fs::exists(csv_file));
}

TEST_F(CsvHandlerTest, ParseQuotedFields) {
    lithium::target::io::CsvHandler handler;

    // Create CSV with quoted fields
    std::string csv_content =
        "name,description\n"
        "\"field1\",\"A, complex, field\"\n"
        "field2,simple value\n";

    auto csv_file = test_dir / "quoted.csv";
    std::ofstream out(csv_file);
    out << csv_content;
    out.close();

    auto result = handler.read(csv_file.string());
    EXPECT_TRUE(result);

    auto records = result.value();
    EXPECT_EQ(records.size(), 2);
    EXPECT_EQ(records[0]["description"], "A, complex, field");
}

TEST_F(CsvHandlerTest, CustomDialect) {
    lithium::target::io::CsvHandler handler;

    // Create semicolon-delimited file
    std::string csv_content =
        "name;value\n"
        "field1;value1\n"
        "field2;value2\n";

    auto csv_file = test_dir / "semicolon.csv";
    std::ofstream out(csv_file);
    out << csv_content;
    out.close();

    // Read with custom dialect
    lithium::target::io::CsvDialect dialect(';', '"', '\\', true, false, "\n",
                                           false);

    auto result = handler.read(csv_file.string(), dialect);
    EXPECT_TRUE(result);

    auto records = result.value();
    EXPECT_EQ(records.size(), 2);
    EXPECT_EQ(records[0]["name"], "field1");
    EXPECT_EQ(records[0]["value"], "value1");
}

TEST_F(CsvHandlerTest, FileNotFound) {
    lithium::target::io::CsvHandler handler;

    auto result = handler.read(
        (test_dir / "nonexistent.csv").string());
    EXPECT_FALSE(result);
    EXPECT_FALSE(result.value_or({}).empty() == false);
}

TEST_F(CsvHandlerTest, EmptyFile) {
    lithium::target::io::CsvHandler handler;

    auto csv_file = test_dir / "empty.csv";
    std::ofstream out(csv_file);
    out.close();

    auto result = handler.read(csv_file.string());
    EXPECT_FALSE(result);
}
