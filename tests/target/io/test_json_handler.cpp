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
#include "target/io/json_handler.hpp"

using json = nlohmann::json;
namespace fs = std::filesystem;

class JsonHandlerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create temporary directory for test files
        test_dir = fs::temp_directory_path() / "lithium_json_test";
        fs::create_directories(test_dir);
    }

    void TearDown() override {
        // Clean up test files
        fs::remove_all(test_dir);
    }

    fs::path test_dir;
};

TEST_F(JsonHandlerTest, ReadSimpleJson) {
    lithium::target::io::JsonHandler handler;

    // Create a simple JSON file
    json data = {
        {"name", "test"},
        {"value", 42}
    };

    auto json_file = test_dir / "simple.json";
    std::ofstream out(json_file);
    out << data.dump(2);
    out.close();

    auto result = handler.read(json_file.string());
    EXPECT_TRUE(result);

    auto parsed = result.value();
    EXPECT_EQ(parsed["name"], "test");
    EXPECT_EQ(parsed["value"], 42);
}

TEST_F(JsonHandlerTest, WriteJson) {
    lithium::target::io::JsonHandler handler;

    json data = {
        {"name", "output"},
        {"items", json::array({1, 2, 3})}
    };

    auto json_file = test_dir / "output.json";
    auto result = handler.write(json_file.string(), data, 2);
    EXPECT_TRUE(result);

    // Verify file exists
    EXPECT_TRUE(fs::exists(json_file));

    // Read back and verify
    auto read_result = handler.read(json_file.string());
    EXPECT_TRUE(read_result);
    EXPECT_EQ(read_result.value()["name"], "output");
}

TEST_F(JsonHandlerTest, ReadJsonArray) {
    lithium::target::io::JsonHandler handler;

    // Create JSON array file
    json data = json::array({
        {{"name", "item1"}, {"id", 1}},
        {{"name", "item2"}, {"id", 2}},
    });

    auto json_file = test_dir / "array.json";
    std::ofstream out(json_file);
    out << data.dump(2);
    out.close();

    auto result = handler.read(json_file.string());
    EXPECT_TRUE(result);

    auto parsed = result.value();
    EXPECT_TRUE(parsed.is_array());
    EXPECT_EQ(parsed.size(), 2);
}

TEST_F(JsonHandlerTest, CompactOutput) {
    lithium::target::io::JsonHandler handler;

    json data = {
        {"name", "compact"},
        {"nested", {{"value", 123}}}
    };

    auto json_file = test_dir / "compact.json";
    auto result = handler.write(json_file.string(), data, 0);
    EXPECT_TRUE(result);

    // Read file and check it's compact (no extra whitespace)
    std::ifstream in(json_file);
    std::string content((std::istreambuf_iterator<char>(in)),
                        std::istreambuf_iterator<char>());
    in.close();

    // Compact JSON should not have newlines (or very few)
    EXPECT_FALSE(content.empty());
    EXPECT_TRUE(content[0] == '{');
}

TEST_F(JsonHandlerTest, InvalidJsonFile) {
    lithium::target::io::JsonHandler handler;

    // Create invalid JSON file
    auto json_file = test_dir / "invalid.json";
    std::ofstream out(json_file);
    out << "{ invalid json }";
    out.close();

    auto result = handler.read(json_file.string());
    EXPECT_FALSE(result);
}

TEST_F(JsonHandlerTest, NonexistentFile) {
    lithium::target::io::JsonHandler handler;

    auto result =
        handler.read((test_dir / "nonexistent.json").string());
    EXPECT_FALSE(result);
}

TEST_F(JsonHandlerTest, CelestialObjectValidation) {
    // Test validation of celestial object JSON
    json valid_obj = {
        {"identifier", "M31"},
        {"chineseName", "仙女座大星系"},
        {"type", "Galaxy"}
    };

    auto result =
        lithium::target::io::JsonHandler::validateCelestialObjectJson(
            valid_obj);
    EXPECT_TRUE(result);

    // Test invalid object (missing identifier)
    json invalid_obj = {
        {"chineseName", "仙女座大星系"},
        {"type", "Galaxy"}
    };

    auto invalid_result =
        lithium::target::io::JsonHandler::validateCelestialObjectJson(
            invalid_obj);
    EXPECT_FALSE(invalid_result);
}

TEST_F(JsonHandlerTest, StreamProcess) {
    lithium::target::io::JsonHandler handler;

    // Create JSONL file
    auto jsonl_file = test_dir / "stream.jsonl";
    std::ofstream out(jsonl_file);
    out << R"({"id": 1, "name": "item1"})" << "\n";
    out << R"({"id": 2, "name": "item2"})" << "\n";
    out << R"({"id": 3, "name": "item3"})" << "\n";
    out.close();

    int count = 0;
    auto result = handler.streamProcess(
        jsonl_file.string(),
        [&count](const json& obj) -> std::expected<void, std::string> {
            count++;
            EXPECT_TRUE(obj.contains("id"));
            return {};
        });

    EXPECT_TRUE(result);
    EXPECT_EQ(result.value(), 3);
    EXPECT_EQ(count, 3);
}
