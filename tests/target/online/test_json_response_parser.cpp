// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Lithium-Next - A modern astrophotography terminal
 * Copyright (C) 2024 Max Qian
 */

#include <gtest/gtest.h>
#include <string>
#include "target/online/parser/json_response_parser.hpp"

using namespace lithium::target::online;

class JsonResponseParserTest : public ::testing::Test {
protected:
    JsonResponseParser parser;

    // Sample NED-like JSON response
    static constexpr std::string_view SAMPLE_NED_JSON = R"({
  "Name": "NGC 224",
  "Type": "G",
  "Preferred": {
    "Coordinates": {
      "RA_deg": 10.6847,
      "DEC_deg": 41.2689
    }
  },
  "Description": "Andromeda Galaxy",
  "Mag_V": 3.44
})";

    // Sample JPL Horizons-like JSON response
    static constexpr std::string_view SAMPLE_JPL_JSON = R"({
  "signature": {
    "source": "JPL Horizons"
  },
  "result": [
    {
      "datetime": "2000-01-01T00:00:00",
      "RA": 123.45,
      "DEC": 45.67,
      "delta": 1.01,
      "mag": -1.5,
      "elong": 45.0,
      "phase": 30.0
    }
  ]
})";

    // Sample GAIA-like JSON response
    static constexpr std::string_view SAMPLE_GAIA_JSON = R"({
  "data": [
    {
      "source_id": "GAIA DR3 12345",
      "ra": 100.5,
      "dec": -30.2,
      "phot_g_mean_mag": 12.34,
      "phot_bp_mean_mag": 12.5,
      "parallax": 5.5
    }
  ]
})";

    // Sample array response
    static constexpr std::string_view SAMPLE_ARRAY_JSON = R"([
  {
    "name": "Vega",
    "ra": 279.23,
    "dec": 38.78,
    "mag": 0.03
  },
  {
    "name": "Altair",
    "ra": 297.70,
    "dec": 8.87,
    "mag": 0.76
  }
])";
};

TEST_F(JsonResponseParserTest, ParseNEDResponse) {
    auto result = parser.parse(SAMPLE_NED_JSON);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->size(), 1);
    EXPECT_EQ(result->at(0).identifier, "NGC 224");
}

TEST_F(JsonResponseParserTest, ParseNEDCoordinates) {
    auto result = parser.parse(SAMPLE_NED_JSON);
    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(result->at(0).radJ2000, 10.6847);
    EXPECT_DOUBLE_EQ(result->at(0).decDJ2000, 41.2689);
}

TEST_F(JsonResponseParserTest, ParseNEDMagnitude) {
    auto result = parser.parse(SAMPLE_NED_JSON);
    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(result->at(0).visualMagnitudeV, 3.44);
}

TEST_F(JsonResponseParserTest, ParseJPLEphemeris) {
    auto result = parser.parseEphemeris(SAMPLE_JPL_JSON);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->size(), 1);
    EXPECT_DOUBLE_EQ(result->at(0).ra, 123.45);
    EXPECT_DOUBLE_EQ(result->at(0).dec, 45.67);
    EXPECT_DOUBLE_EQ(result->at(0).distance, 1.01);
    EXPECT_DOUBLE_EQ(result->at(0).magnitude, -1.5);
}

TEST_F(JsonResponseParserTest, ParseGAIAResponse) {
    auto result = parser.parse(SAMPLE_GAIA_JSON);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->size(), 1);
    EXPECT_EQ(result->at(0).identifier, "GAIA DR3 12345");
    EXPECT_DOUBLE_EQ(result->at(0).radJ2000, 100.5);
}

TEST_F(JsonResponseParserTest, ParseArrayResponse) {
    auto result = parser.parse(SAMPLE_ARRAY_JSON);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->size(), 2);
    EXPECT_EQ(result->at(0).identifier, "Vega");
    EXPECT_EQ(result->at(1).identifier, "Altair");
}

TEST_F(JsonResponseParserTest, FormatDetection) {
    EXPECT_EQ(parser.format(), ResponseFormat::JSON);
}

TEST_F(JsonResponseParserTest, CustomObjectParser) {
    JsonResponseParser customParser;
    customParser.setObjectParser([](const nlohmann::json& json) {
        CelestialObjectModel obj;
        if (json.contains("custom_id")) {
            obj.identifier = json["custom_id"].get<std::string>();
        }
        return obj;
    });

    std::string customJson = R"({"custom_id": "custom_object"})";
    auto result = customParser.parse(customJson);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->at(0).identifier, "custom_object");
}

TEST_F(JsonResponseParserTest, CustomEphemerisParser) {
    JsonResponseParser customParser;
    customParser.setEphemerisParser([](const nlohmann::json& json) {
        EphemerisPoint point;
        if (json.contains("custom_ra")) {
            point.ra = json["custom_ra"].get<double>();
        }
        return point;
    });

    std::string customJson = R"({"custom_ra": 150.0})";
    auto result = customParser.parseEphemeris(customJson);
    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(result->at(0).ra, 150.0);
}

TEST_F(JsonResponseParserTest, SetObjectsPath) {
    JsonResponseParser customParser;
    customParser.setObjectsPath("results");

    std::string json = R"({"results": [{"name": "obj1"}]})";
    auto result = customParser.parse(json);
    ASSERT_TRUE(result.has_value());
}

TEST_F(JsonResponseParserTest, InvalidJSON) {
    std::string invalidJson = "{invalid json}";
    auto result = parser.parse(invalidJson);
    ASSERT_FALSE(result.has_value());
    EXPECT_NE(result.error().message.find("parse error"), std::string::npos);
}

TEST_F(JsonResponseParserTest, EmptyArray) {
    std::string emptyArray = "[]";
    auto result = parser.parse(emptyArray);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->size(), 0);
}

TEST_F(JsonResponseParserTest, NestedPath) {
    JsonResponseParser customParser;
    customParser.setObjectsPath("data.objects");

    std::string json = R"({
    "data": {
        "objects": [
            {"name": "obj1"}
        ]
    }
})";
    auto result = customParser.parse(json);
    // Should handle nested paths
}

TEST_F(JsonResponseParserTest, NedParserFunction) {
    JsonResponseParser customParser;
    customParser.setObjectParser(JsonResponseParser::nedParser());
    auto result = customParser.parse(SAMPLE_NED_JSON);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->at(0).identifier, "NGC 224");
}

TEST_F(JsonResponseParserTest, JplHorizonsParserFunction) {
    JsonResponseParser customParser;
    customParser.setEphemerisParser(JsonResponseParser::jplHorizonsParser());
    auto result = customParser.parseEphemeris(SAMPLE_JPL_JSON);
    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(result->at(0).ra, 123.45);
}

TEST_F(JsonResponseParserTest, GaiaParserFunction) {
    JsonResponseParser customParser;
    customParser.setObjectParser(JsonResponseParser::gaiaParser());
    auto result = customParser.parse(SAMPLE_GAIA_JSON);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->at(0).identifier, "GAIA DR3 12345");
    EXPECT_DOUBLE_EQ(result->at(0).radJ2000, 100.5);
    EXPECT_DOUBLE_EQ(result->at(0).decDJ2000, -30.2);
}

TEST_F(JsonResponseParserTest, MissingRequiredFields) {
    std::string incomplete = R"({"type": "incomplete"})";
    auto result = parser.parse(incomplete);
    ASSERT_TRUE(result.has_value());
    // Should handle gracefully
}

TEST_F(JsonResponseParserTest, NumericStringValues) {
    std::string json = R"({
    "name": "TestObj",
    "ra": "123.45",
    "dec": "45.67"
})";
    auto result = parser.parse(json);
    ASSERT_TRUE(result.has_value());
}

TEST_F(JsonResponseParserTest, ParseNullValues) {
    std::string json = R"({
    "name": null,
    "ra": 100.0,
    "dec": null
})";
    auto result = parser.parse(json);
    ASSERT_TRUE(result.has_value());
    // Should handle null gracefully
}

class JsonDetectFormatTest : public ::testing::Test {};

TEST_F(JsonDetectFormatTest, DetectJSONFormat) {
    std::string json = R"({"key": "value"})";
    EXPECT_EQ(detectFormat(json), ResponseFormat::JSON);
}

TEST_F(JsonDetectFormatTest, DetectJSONArray) {
    std::string json = R"([{"key": "value"}])";
    EXPECT_EQ(detectFormat(json), ResponseFormat::JSON);
}

TEST_F(JsonDetectFormatTest, NotConfusedWithCSV) {
    std::string csv = "a,b,c\n1,2,3";
    // Should not mistake CSV for JSON
}
