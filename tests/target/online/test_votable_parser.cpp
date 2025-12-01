// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Lithium-Next - A modern astrophotography terminal
 * Copyright (C) 2024 Max Qian
 */

#include <gtest/gtest.h>
#include <string>
#include "target/online/parser/votable_parser.hpp"

using namespace lithium::target::online;

class VotableParserTest : public ::testing::Test {
protected:
    VotableParser parser;

    // Sample SIMBAD-like VOTable response
    static constexpr std::string_view SAMPLE_VOTABLE = R"(<?xml version="1.0" encoding="UTF-8"?>
<VOTABLE version="1.3" xmlns="http://www.ivoa.net/xml/VOTable/v1.3"
 xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
 xsi:schemaLocation="http://www.ivoa.net/xml/VOTable/v1.3
  http://www.ivoa.net/xml/VOTable/v1.3/VOTable.xsd">
 <RESOURCE type="results">
  <TABLE name="results">
   <FIELD name="main_id" datatype="char" arraysize="*" ucd="meta.id;meta.main"/>
   <FIELD name="RA_ICRS_Angle_alpha" datatype="double" unit="deg" ucd="pos.eq.ra;meta.main"/>
   <FIELD name="DEC_ICRS_Angle_delta" datatype="double" unit="deg" ucd="pos.eq.dec;meta.main"/>
   <FIELD name="V" datatype="double" unit="mag" ucd="phot.mag;em.opt.SDSS.i"/>
   <FIELD name="B" datatype="double" unit="mag" ucd="phot.mag;em.opt.SDSS.u"/>
   <FIELD name="Const" datatype="char" arraysize="3"/>
   <DATA>
    <TABLEDATA>
     <TR>
      <TD>Polaris A</TD>
      <TD>37.95456067</TD>
      <TD>89.26414250</TD>
      <TD>2.00</TD>
      <TD>2.01</TD>
      <TD>Ursa Minor</TD>
     </TR>
     <TR>
      <TD>Vega</TD>
      <TD>279.23473479</TD>
      <TD>38.78368896</TD>
      <TD>0.03</TD>
      <TD>0.03</TD>
      <TD>Lyra</TD>
     </TR>
    </TABLEDATA>
   </DATA>
  </TABLE>
 </RESOURCE>
</VOTABLE>)";

    // Sample ephemeris VOTable
    static constexpr std::string_view SAMPLE_EPHEMERIS = R"(<?xml version="1.0" encoding="UTF-8"?>
<VOTABLE version="1.3">
 <RESOURCE>
  <TABLE>
   <FIELD name="DATE__1" datatype="char" arraysize="*"/>
   <FIELD name="RA_ICRS" datatype="double" unit="deg"/>
   <FIELD name="DEC_ICRS" datatype="double" unit="deg"/>
   <FIELD name="Delta" datatype="double" unit="AU"/>
   <FIELD name="Mag" datatype="double" unit="mag"/>
   <FIELD name="Elong" datatype="double" unit="deg"/>
   <DATA>
    <TABLEDATA>
     <TR>
      <TD>2000-01-01T00:00:00</TD>
      <TD>123.45</TD>
      <TD>45.67</TD>
      <TD>1.01</TD>
      <TD>-1.5</TD>
      <TD>45.0</TD>
     </TR>
    </TABLEDATA>
   </DATA>
  </TABLE>
 </RESOURCE>
</VOTABLE>)";
};

TEST_F(VotableParserTest, ParseValidVOTable) {
    auto result = parser.parse(SAMPLE_VOTABLE);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->size(), 2);
    EXPECT_EQ(result->at(0).identifier, "Polaris A");
    EXPECT_EQ(result->at(1).identifier, "Vega");
}

TEST_F(VotableParserTest, ParseCoordinates) {
    auto result = parser.parse(SAMPLE_VOTABLE);
    ASSERT_TRUE(result.has_value());
    EXPECT_GT(result->at(0).radJ2000, 0.0);
    EXPECT_GT(result->at(0).decDJ2000, 0.0);
    EXPECT_EQ(result->at(0).constellationEn, "Ursa Minor");
}

TEST_F(VotableParserTest, ParseMagnitudes) {
    auto result = parser.parse(SAMPLE_VOTABLE);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->at(0).visualMagnitudeV, 2.00);
    EXPECT_EQ(result->at(0).photographicMagnitudeB, 2.01);
    EXPECT_EQ(result->at(1).visualMagnitudeV, 0.03);
}

TEST_F(VotableParserTest, ParseEphemeris) {
    auto result = parser.parseEphemeris(SAMPLE_EPHEMERIS);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->size(), 1);
    EXPECT_EQ(result->at(0).ra, 123.45);
    EXPECT_EQ(result->at(0).dec, 45.67);
    EXPECT_EQ(result->at(0).distance, 1.01);
    EXPECT_EQ(result->at(0).magnitude, -1.5);
}

TEST_F(VotableParserTest, FormatDetection) {
    EXPECT_EQ(parser.format(), ResponseFormat::VOTable);
}

TEST_F(VotableParserTest, CustomFieldMappings) {
    std::vector<VotableFieldMapping> mappings = {
        {"main_id", "identifier", std::nullopt},
        {"RA_ICRS_Angle_alpha", "raJ2000", std::nullopt},
    };
    parser.setFieldMappings(mappings);
    auto result = parser.parse(SAMPLE_VOTABLE);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->at(0).identifier, "Polaris A");
}

TEST_F(VotableParserTest, InvalidXMLStructure) {
    std::string invalid = "<invalid>no votable</invalid>";
    auto result = parser.parse(invalid);
    ASSERT_FALSE(result.has_value());
    EXPECT_NE(result.error().message.find("No RESOURCE"), std::string::npos);
}

TEST_F(VotableParserTest, EmptyTableData) {
    std::string emptyTable = R"(<?xml version="1.0"?>
<VOTABLE version="1.3">
 <RESOURCE>
  <TABLE>
   <FIELD name="id" datatype="char"/>
   <DATA>
    <TABLEDATA>
    </TABLEDATA>
   </DATA>
  </TABLE>
 </RESOURCE>
</VOTABLE>)";
    auto result = parser.parse(emptyTable);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->size(), 0);
}

TEST_F(VotableParserTest, SimbadMappings) {
    auto mappings = VotableParser::simbadMappings();
    EXPECT_GT(mappings.size(), 0);
    EXPECT_EQ(mappings[0].votableField, "main_id");
    EXPECT_EQ(mappings[0].modelField, "identifier");
}

TEST_F(VotableParserTest, VizierNGCMappings) {
    auto mappings = VotableParser::vizierNgcMappings();
    EXPECT_GT(mappings.size(), 0);
    EXPECT_EQ(mappings[0].votableField, "Name");
}

TEST_F(VotableParserTest, FormatDetectionFunction) {
    EXPECT_EQ(detectFormat(SAMPLE_VOTABLE), ResponseFormat::VOTable);
    EXPECT_EQ(detectFormat(R"({"key": "value"})"), ResponseFormat::JSON);
    EXPECT_EQ(detectFormat("invalid"), ResponseFormat::Unknown);
}

TEST_F(VotableParserTest, SexagesimalCoordinates) {
    std::string sexagesimal = R"(<?xml version="1.0"?>
<VOTABLE version="1.3">
 <RESOURCE>
  <TABLE>
   <FIELD name="name" datatype="char"/>
   <FIELD name="RA" datatype="char"/>
   <FIELD name="DEC" datatype="char"/>
   <DATA>
    <TABLEDATA>
     <TR>
      <TD>TestObject</TD>
      <TD>12:30:45.5</TD>
      <TD>+45:30:15.2</TD>
     </TR>
    </TABLEDATA>
   </DATA>
  </TABLE>
 </RESOURCE>
</VOTABLE>)";
    auto result = parser.parse(sexagesimal);
    ASSERT_TRUE(result.has_value());
    EXPECT_GT(result->at(0).radJ2000, 0.0);
    EXPECT_GT(result->at(0).decDJ2000, 0.0);
}

class DetectFormatTest : public ::testing::Test {};

TEST_F(DetectFormatTest, DetectVOTable) {
    std::string votable = "<?xml version=\"1.0\"?>\n<VOTABLE>";
    EXPECT_EQ(detectFormat(votable), ResponseFormat::VOTable);
}

TEST_F(DetectFormatTest, DetectJSON) {
    std::string json = R"({"key": "value"})";
    EXPECT_EQ(detectFormat(json), ResponseFormat::JSON);
}

TEST_F(DetectFormatTest, DetectCSV) {
    std::string csv = "name,ra,dec\nVega,279.23,38.78";
    EXPECT_EQ(detectFormat(csv), ResponseFormat::CSV);
}

TEST_F(DetectFormatTest, UnknownFormat) {
    std::string unknown = "some random text";
    EXPECT_EQ(detectFormat(unknown), ResponseFormat::Unknown);
}
