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
#include <fstream>

#include "atom/type/json.hpp"
#include "target/io/csv_handler.hpp"
#include "target/io/json_handler.hpp"

using json = nlohmann::json;
namespace fs = std::filesystem;

class IntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir = fs::temp_directory_path() / "lithium_io_integration";
        fs::create_directories(test_dir);
    }

    void TearDown() override {
        fs::remove_all(test_dir);
    }

    fs::path test_dir;
};

TEST_F(IntegrationTest, CsvToJsonConversion) {
    lithium::target::io::CsvHandler csv_handler;
    lithium::target::io::JsonHandler json_handler;

    // Create CSV file with celestial objects
    std::string csv_content =
        "identifier,chineseName,type,radJ2000,decDJ2000\n"
        "M31,仙女座大星系,Galaxy,0.19086,41.26906\n"
        "M51,漩涡星系,Galaxy,202.469,47.195\n";

    auto csv_file = test_dir / "input.csv";
    std::ofstream csv_out(csv_file);
    csv_out << csv_content;
    csv_out.close();

    // Import from CSV
    auto csv_result = csv_handler.importCelestialObjects(csv_file.string());
    EXPECT_TRUE(csv_result);

    auto [csv_objects, csv_stats] = csv_result.value();
    EXPECT_EQ(csv_stats.totalRecords, 2);
    EXPECT_EQ(csv_stats.successCount, 2);

    // Export to JSON
    auto json_file = test_dir / "output.json";
    auto export_result = json_handler.exportCelestialObjects(
        json_file.string(), csv_objects, true, 2);
    EXPECT_TRUE(export_result);

    // Verify JSON file can be read back
    auto json_import_result =
        json_handler.importCelestialObjects(json_file.string());
    EXPECT_TRUE(json_import_result);

    auto [json_objects, json_stats] = json_import_result.value();
    EXPECT_EQ(json_objects.size(), 2);
    EXPECT_EQ(json_objects[0].identifier, "M31");
    EXPECT_EQ(json_objects[1].identifier, "M51");
}

TEST_F(IntegrationTest, JsonToJsonLConversion) {
    lithium::target::io::JsonHandler handler;

    // Create JSON array
    json data = json::array({
        {{"identifier", "M31"}, {"type", "Galaxy"}},
        {{"identifier", "M51"}, {"type", "Galaxy"}},
    });

    // Export as array
    auto array_file = test_dir / "array.json";
    auto array_result =
        handler.write(array_file.string(), data, 2);
    EXPECT_TRUE(array_result);

    // Read back
    auto read_result = handler.read(array_file.string());
    EXPECT_TRUE(read_result);
    EXPECT_TRUE(read_result.value().is_array());
}

TEST_F(IntegrationTest, CsvRoundTrip) {
    lithium::target::io::CsvHandler handler;

    // Create sample data
    std::vector<std::unordered_map<std::string, std::string>> data = {
        {{"id", "1"}, {"name", "Object1"}, {"value", "100"}},
        {{"id", "2"}, {"name", "Object2"}, {"value", "200"}},
    };

    std::vector<std::string> fields = {"id", "name", "value"};

    // Write CSV
    auto csv_file = test_dir / "roundtrip.csv";
    auto write_result =
        handler.write(csv_file.string(), data, fields);
    EXPECT_TRUE(write_result);
    EXPECT_EQ(write_result.value(), 2);

    // Read back
    auto read_result = handler.read(csv_file.string());
    EXPECT_TRUE(read_result);

    auto records = read_result.value();
    EXPECT_EQ(records.size(), 2);
    EXPECT_EQ(records[0]["id"], "1");
    EXPECT_EQ(records[0]["name"], "Object1");
    EXPECT_EQ(records[0]["value"], "100");
    EXPECT_EQ(records[1]["id"], "2");
    EXPECT_EQ(records[1]["name"], "Object2");
    EXPECT_EQ(records[1]["value"], "200");
}

TEST_F(IntegrationTest, ErrorHandling) {
    lithium::target::io::CsvHandler csv_handler;
    lithium::target::io::JsonHandler json_handler;

    // Try to read non-existent CSV file
    auto csv_result =
        csv_handler.read((test_dir / "nonexistent.csv").string());
    EXPECT_FALSE(csv_result);
    EXPECT_FALSE(csv_result.error().empty());

    // Try to read invalid JSON
    auto invalid_json = test_dir / "invalid.json";
    std::ofstream out(invalid_json);
    out << "{ this is not valid json }";
    out.close();

    auto json_result = json_handler.read(invalid_json.string());
    EXPECT_FALSE(json_result);
    EXPECT_FALSE(json_result.error().empty());
}

TEST_F(IntegrationTest, SpecialCharactersInCsv) {
    lithium::target::io::CsvHandler handler;

    // CSV with special characters
    std::string csv_content =
        "name,description\n"
        "\"Field 1\",\"Contains, comma\"\n"
        "\"Field 2\",\"Contains \"\"quotes\"\"\"\n"
        "\"Field 3\",\"Multi\nline\nfield\"\n";

    auto csv_file = test_dir / "special.csv";
    std::ofstream out(csv_file);
    out << csv_content;
    out.close();

    auto result = handler.read(csv_file.string());
    EXPECT_TRUE(result);

    auto records = result.value();
    EXPECT_EQ(records.size(), 3);
    EXPECT_EQ(records[0]["description"], "Contains, comma");
    EXPECT_EQ(records[1]["description"], "Contains \"quotes\"");
}
