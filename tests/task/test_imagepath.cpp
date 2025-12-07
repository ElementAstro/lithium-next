#ifndef LITHIUM_TASK_TEST_IMAGEPATH_HPP
#define LITHIUM_TASK_TEST_IMAGEPATH_HPP

#include <gtest/gtest.h>
#include <filesystem>
#include "atom/type/json.hpp"
#include "task/utils/imagepath.hpp"

namespace lithium::test {

class ImageInfoTest : public ::testing::Test {
protected:
    ImageInfo info;

    void SetUp() override {
        info.path = "/test/image.fits";
        info.dateTime = "2023-01-01-12-00-00";
        info.imageType = "LIGHT";
        info.filter = "R";
        info.sensorTemp = "-10C";
        info.exposureTime = "300";
        info.frameNr = "001";
    }
};

TEST_F(ImageInfoTest, JsonSerialization) {
    auto json = info.toJson();
    auto decoded = ImageInfo::fromJson(json);
    EXPECT_EQ(info, decoded);
}

TEST_F(ImageInfoTest, HashComputation) {
    size_t hash1 = info.hash();
    info.path = "/test/image2.fits";
    size_t hash2 = info.hash();
    EXPECT_NE(hash1, hash2);
}

TEST_F(ImageInfoTest, CompletenessCheck) {
    EXPECT_TRUE(info.isComplete());
    info.dateTime = std::nullopt;
    EXPECT_FALSE(info.isComplete());
}

TEST_F(ImageInfoTest, Merging) {
    ImageInfo other;
    other.cameraModel = "TestCam";
    other.gain = 100;
    info.mergeWith(other);
    EXPECT_EQ(info.cameraModel, "TestCam");
    EXPECT_EQ(info.gain, 100);
}

class ImagePatternParserTest : public ::testing::Test {
protected:
    ImagePatternParser parser{"$DATETIME_$IMAGETYPE_$FILTER_$EXPOSURETIME"};

    void SetUp() override { parser.enableCache(100); }
};

TEST_F(ImagePatternParserTest, BasicParsing) {
    auto result = parser.parseFilename("2023-01-01-12-00-00_LIGHT_R_300.fits");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->dateTime, "2023-01-01-12-00-00");
    EXPECT_EQ(result->imageType, "LIGHT");
    EXPECT_EQ(result->filter, "R");
    EXPECT_EQ(result->exposureTime, "300");
}

TEST_F(ImagePatternParserTest, CustomFieldParser) {
    parser.addCustomParser(
        "EXPOSURETIME", [](ImageInfo& info, const std::string& value) {
            info.exposureTime = std::to_string(std::stoi(value) / 1000.0);
        });

    auto result = parser.parseFilename("2023-01-01-12-00-00_LIGHT_R_1000.fits");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->exposureTime, "1");
}

TEST_F(ImagePatternParserTest, OptionalFields) {
    parser.setOptionalField("FILTER", "L");
    auto result = parser.parseFilename("2023-01-01-12-00-00_LIGHT__300.fits");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->filter, "L");
}

TEST_F(ImagePatternParserTest, FieldValidation) {
    parser.setFieldValidator("EXPOSURETIME", [](const std::string& value) {
        return std::stoi(value) > 0;
    });

    auto result = parser.parseFilename("2023-01-01-12-00-00_LIGHT_R_-1.fits");
    EXPECT_FALSE(result.has_value());
}

TEST_F(ImagePatternParserTest, CacheFunctionality) {
    const std::string filename = "2023-01-01-12-00-00_LIGHT_R_300.fits";

    auto result1 = parser.parseFilename(filename);
    auto result2 = parser.parseFilename(filename);

    ASSERT_TRUE(result1.has_value());
    ASSERT_TRUE(result2.has_value());
    EXPECT_EQ(*result1, *result2);
}

TEST_F(ImagePatternParserTest, AsyncParsing) {
    auto future =
        parser.parseFilenameAsync("2023-01-01-12-00-00_LIGHT_R_300.fits");
    auto result = future.get();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->dateTime, "2023-01-01-12-00-00");
}

TEST_F(ImagePatternParserTest, BatchProcessing) {
    std::vector<std::string> filenames = {
        "2023-01-01-12-00-00_LIGHT_R_300.fits",
        "2023-01-01-12-01-00_LIGHT_R_300.fits"};

    auto results = parser.parseFilenames(filenames);
    EXPECT_EQ(results.size(), 2);
    EXPECT_TRUE(results[0].has_value());
    EXPECT_TRUE(results[1].has_value());
}

TEST_F(ImagePatternParserTest, ErrorHandling) {
    bool errorCaught = false;
    parser.setErrorHandler(
        [&errorCaught](const std::string&) { errorCaught = true; });

    parser.parseFilename("invalid_filename.fits");
    EXPECT_TRUE(errorCaught);
}

/*
TODO: Fix filesystem issues
TEST_F(ImagePatternParserTest, FileOperations) {
    // Create temporary test directory and files
    std::filesystem::path testDir =
        std::filesystem::temp_directory_path() / "test_images";
    std::filesystem::create_directory(testDir);

    std::ofstream(testDir / "2023-01-01-12-00-00_LIGHT_R_300.fits").close();
    std::ofstream(testDir / "2023-01-01-12-01-00_LIGHT_R_300.fits").close();
    std::ofstream(testDir / "invalid_file.fits").close();

    // Test with default filter (accepts all files)
    // auto results = parser.findFilesInDirectory<ImagePatternParser>(testDir);
    // EXPECT_EQ(results.size(), 2);  // Should only find 2 valid files

    // Test with custom filter
    auto filtered_results = parser.findFilesInDirectory(
        testDir, [](const std::filesystem::path& p) {
            return p.filename().string().starts_with("2023-01-01-12-00-00");
        });
    EXPECT_EQ(filtered_results.size(), 1);

    std::filesystem::remove_all(testDir);
}
*/

TEST_F(ImagePatternParserTest, PatternValidation) {
    EXPECT_TRUE(
        parser.validatePattern("$DATETIME_$IMAGETYPE_$FILTER_$EXPOSURETIME"));
    EXPECT_FALSE(parser.validatePattern("invalid_pattern"));
}

TEST_F(ImagePatternParserTest, FileNaming) {
    auto namer =
        parser.createFileNamer("$DATETIME_$IMAGETYPE_$FILTER_$EXPOSURETIME");

    ImageInfo info;
    info.dateTime = "2023-01-01-12-00-00";
    info.imageType = "LIGHT";
    info.filter = "R";
    info.exposureTime = "300";

    std::string filename = namer(info);
    EXPECT_EQ(filename, "2023-01-01-12-00-00_LIGHT_R_300");
}

TEST_F(ImagePatternParserTest, PreProcessor) {
    parser.setPreProcessor(
        [](std::string filename) { return filename + ".fits"; });

    auto result = parser.parseFilename("2023-01-01-12-00-00_LIGHT_R_300");
    ASSERT_TRUE(result.has_value());
}

}  // namespace lithium::test

#endif  // LITHIUM_TASK_TEST_IMAGEPATH_HPP
