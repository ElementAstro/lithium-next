/*
 * test_image.cpp - Tests for Image Middleware
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <vector>

// Test the internal parsing functions without full middleware dependencies
namespace fs = std::filesystem;

// Replicate internal functions for testing
namespace test_internal {

inline auto parseString(const std::string& input,
                        const std::string& imgFilePath)
    -> std::vector<std::string> {
    std::vector<std::string> paths;
    std::string baseString;

    size_t pos = input.find('{');
    if (pos != std::string::npos) {
        baseString = input.substr(0, pos);
        std::string content = input.substr(pos + 1);

        size_t endPos = content.find('}');
        if (endPos != std::string::npos) {
            content = content.substr(0, endPos);

            if (!content.empty() && content.back() == ';') {
                content.pop_back();
            }

            size_t start = 0;
            size_t end;
            while ((end = content.find(';', start)) != std::string::npos) {
                std::string part = content.substr(start, end - start);
                if (!part.empty()) {
                    paths.push_back(
                        (fs::path(imgFilePath) / baseString / part).string());
                }
                start = end + 1;
            }
            if (start < content.size()) {
                std::string part = content.substr(start);
                if (!part.empty()) {
                    paths.push_back(
                        (fs::path(imgFilePath) / baseString / part).string());
                }
            }
        }
    }
    return paths;
}

inline std::string escapeSpecialChars(const std::string& input) {
    std::string result;
    for (char c : input) {
        switch (c) {
            case ' ':
                result += "\\ ";
                break;
            case '[':
                result += "\\[";
                break;
            case ']':
                result += "\\]";
                break;
            case ',':
                result += "\\,";
                break;
            default:
                result += c;
                break;
        }
    }
    return result;
}

}  // namespace test_internal

// ============================================================================
// ImageFiles Structure Tests
// ============================================================================

class ImageFilesTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ImageFilesTest, EmptyStructure) {
    struct ImageFiles {
        std::vector<std::string> captureFiles;
        std::vector<std::string> scheduleFiles;
    };

    ImageFiles files;
    EXPECT_TRUE(files.captureFiles.empty());
    EXPECT_TRUE(files.scheduleFiles.empty());
}

TEST_F(ImageFilesTest, PopulatedStructure) {
    struct ImageFiles {
        std::vector<std::string> captureFiles;
        std::vector<std::string> scheduleFiles;
    };

    ImageFiles files;
    files.captureFiles = {"image1.fits", "image2.fits"};
    files.scheduleFiles = {"schedule1", "schedule2", "schedule3"};

    EXPECT_EQ(files.captureFiles.size(), 2);
    EXPECT_EQ(files.scheduleFiles.size(), 3);
}

// ============================================================================
// parseString Function Tests
// ============================================================================

class ParseStringTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ParseStringTest, BasicParsing) {
    std::string input = "CaptureImage{file1.fits;file2.fits}";
    std::string basePath = "/home/user/images";

    auto result = test_internal::parseString(input, basePath);

    EXPECT_EQ(result.size(), 2);
}

TEST_F(ParseStringTest, SingleFile) {
    std::string input = "CaptureImage{single.fits}";
    std::string basePath = "/images";

    auto result = test_internal::parseString(input, basePath);

    EXPECT_EQ(result.size(), 1);
}

TEST_F(ParseStringTest, EmptyContent) {
    std::string input = "CaptureImage{}";
    std::string basePath = "/images";

    auto result = test_internal::parseString(input, basePath);

    EXPECT_TRUE(result.empty());
}

TEST_F(ParseStringTest, TrailingSemicolon) {
    std::string input = "CaptureImage{file1.fits;file2.fits;}";
    std::string basePath = "/images";

    auto result = test_internal::parseString(input, basePath);

    EXPECT_EQ(result.size(), 2);
}

TEST_F(ParseStringTest, NoOpenBrace) {
    std::string input = "CaptureImage";
    std::string basePath = "/images";

    auto result = test_internal::parseString(input, basePath);

    EXPECT_TRUE(result.empty());
}

TEST_F(ParseStringTest, NoCloseBrace) {
    std::string input = "CaptureImage{file1.fits;file2.fits";
    std::string basePath = "/images";

    auto result = test_internal::parseString(input, basePath);

    EXPECT_TRUE(result.empty());
}

TEST_F(ParseStringTest, MultipleFiles) {
    std::string input = "ScheduleImage{dir1;dir2;dir3;dir4;dir5}";
    std::string basePath = "/data";

    auto result = test_internal::parseString(input, basePath);

    EXPECT_EQ(result.size(), 5);
}

TEST_F(ParseStringTest, EmptyBasePath) {
    std::string input = "CaptureImage{file.fits}";
    std::string basePath = "";

    auto result = test_internal::parseString(input, basePath);

    EXPECT_EQ(result.size(), 1);
}

TEST_F(ParseStringTest, WindowsStylePath) {
    std::string input = "CaptureImage{file1.fits;file2.fits}";
    std::string basePath = "C:\\Users\\test\\images";

    auto result = test_internal::parseString(input, basePath);

    EXPECT_EQ(result.size(), 2);
}

// ============================================================================
// escapeSpecialChars Function Tests
// ============================================================================

class EscapeSpecialCharsTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(EscapeSpecialCharsTest, NoSpecialChars) {
    std::string input = "normalpath/to/file.fits";
    auto result = test_internal::escapeSpecialChars(input);

    EXPECT_EQ(result, input);
}

TEST_F(EscapeSpecialCharsTest, SpaceEscaping) {
    std::string input = "path with spaces";
    auto result = test_internal::escapeSpecialChars(input);

    EXPECT_EQ(result, "path\\ with\\ spaces");
}

TEST_F(EscapeSpecialCharsTest, BracketEscaping) {
    std::string input = "file[1].fits";
    auto result = test_internal::escapeSpecialChars(input);

    EXPECT_EQ(result, "file\\[1\\].fits");
}

TEST_F(EscapeSpecialCharsTest, CommaEscaping) {
    std::string input = "file,name.fits";
    auto result = test_internal::escapeSpecialChars(input);

    EXPECT_EQ(result, "file\\,name.fits");
}

TEST_F(EscapeSpecialCharsTest, MultipleSpecialChars) {
    std::string input = "path [with] special, chars";
    auto result = test_internal::escapeSpecialChars(input);

    EXPECT_EQ(result, "path\\ \\[with\\]\\ special\\,\\ chars");
}

TEST_F(EscapeSpecialCharsTest, EmptyString) {
    std::string input = "";
    auto result = test_internal::escapeSpecialChars(input);

    EXPECT_EQ(result, "");
}

TEST_F(EscapeSpecialCharsTest, OnlySpecialChars) {
    std::string input = " [],";
    auto result = test_internal::escapeSpecialChars(input);

    EXPECT_EQ(result, "\\ \\[\\]\\,");
}

TEST_F(EscapeSpecialCharsTest, ConsecutiveSpaces) {
    std::string input = "path  with   spaces";
    auto result = test_internal::escapeSpecialChars(input);

    EXPECT_EQ(result, "path\\ \\ with\\ \\ \\ spaces");
}

TEST_F(EscapeSpecialCharsTest, NestedBrackets) {
    std::string input = "file[[nested]].fits";
    auto result = test_internal::escapeSpecialChars(input);

    EXPECT_EQ(result, "file\\[\\[nested\\]\\].fits");
}

// ============================================================================
// getAllFiles Format Tests
// ============================================================================

class GetAllFilesFormatTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(GetAllFilesFormatTest, EmptyResultFormat) {
    std::string emptyResult = "CaptureImage{}:ScheduleImage{}";

    // Verify format
    EXPECT_NE(emptyResult.find("CaptureImage{"), std::string::npos);
    EXPECT_NE(emptyResult.find("}:ScheduleImage{"), std::string::npos);
    EXPECT_NE(emptyResult.find("}"), std::string::npos);
}

TEST_F(GetAllFilesFormatTest, ResultWithFiles) {
    std::string result =
        "CaptureImage{img1.fits;img2.fits;}:ScheduleImage{plan1;plan2;}";

    // Parse capture files
    size_t captureStart = result.find("CaptureImage{") + 13;
    size_t captureEnd = result.find("}:");
    std::string captureContent =
        result.substr(captureStart, captureEnd - captureStart);

    EXPECT_FALSE(captureContent.empty());
    EXPECT_NE(captureContent.find("img1.fits"), std::string::npos);
}

// ============================================================================
// Path Construction Tests
// ============================================================================

class PathConstructionTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(PathConstructionTest, CaptureImagePath) {
    fs::path basePath = "/home/user/images";
    fs::path capturePath = basePath / "CaptureImage";

    EXPECT_EQ(capturePath.filename(), "CaptureImage");
}

TEST_F(PathConstructionTest, ScheduleImagePath) {
    fs::path basePath = "/home/user/images";
    fs::path schedulePath = basePath / "ScheduleImage";

    EXPECT_EQ(schedulePath.filename(), "ScheduleImage");
}

TEST_F(PathConstructionTest, FullFilePath) {
    fs::path basePath = "/home/user/images";
    fs::path fullPath = basePath / "CaptureImage" / "image_001.fits";

    EXPECT_EQ(fullPath.filename(), "image_001.fits");
    EXPECT_EQ(fullPath.parent_path().filename(), "CaptureImage");
}

// ============================================================================
// USB Path Tests
// ============================================================================

class USBPathTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(USBPathTest, USBMountPointFormat) {
    std::string user = "testuser";
    std::string basePath = "/media/" + user;

    EXPECT_EQ(basePath, "/media/testuser");
}

TEST_F(USBPathTest, USBDestinationPath) {
    std::string usbMountPoint = "/media/user/USB_DRIVE";
    fs::path destPath = fs::path(usbMountPoint) / "QUARCS_ImageSave";

    EXPECT_EQ(destPath.filename(), "QUARCS_ImageSave");
}

TEST_F(USBPathTest, FileDestinationPath) {
    std::string usbMountPoint = "/media/user/USB_DRIVE";
    fs::path sourcePath = "/home/user/images/CaptureImage/image.fits";
    fs::path destPath =
        fs::path(usbMountPoint) / "QUARCS_ImageSave" / sourcePath.filename();

    EXPECT_EQ(destPath.filename(), "image.fits");
}

// ============================================================================
// Edge Cases
// ============================================================================

class ImageMiddlewareEdgeCasesTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ImageMiddlewareEdgeCasesTest, VeryLongFilename) {
    std::string longName(255, 'a');
    longName += ".fits";

    std::string input = "CaptureImage{" + longName + "}";
    auto result = test_internal::parseString(input, "/images");

    EXPECT_EQ(result.size(), 1);
}

TEST_F(ImageMiddlewareEdgeCasesTest, UnicodeFilename) {
    std::string input = "CaptureImage{文件名.fits}";
    auto result = test_internal::parseString(input, "/images");

    EXPECT_EQ(result.size(), 1);
}

TEST_F(ImageMiddlewareEdgeCasesTest, FilenameWithDots) {
    std::string input = "CaptureImage{image.2024.01.01.fits}";
    auto result = test_internal::parseString(input, "/images");

    EXPECT_EQ(result.size(), 1);
}

TEST_F(ImageMiddlewareEdgeCasesTest, FilenameWithHyphens) {
    std::string input = "CaptureImage{image-001-dark-frame.fits}";
    auto result = test_internal::parseString(input, "/images");

    EXPECT_EQ(result.size(), 1);
}

TEST_F(ImageMiddlewareEdgeCasesTest, FilenameWithUnderscores) {
    std::string input = "CaptureImage{M31_Ha_300s_001.fits}";
    auto result = test_internal::parseString(input, "/images");

    EXPECT_EQ(result.size(), 1);
}

TEST_F(ImageMiddlewareEdgeCasesTest, MixedSeparators) {
    std::string input = "CaptureImage{file1.fits;;file2.fits}";
    auto result = test_internal::parseString(input, "/images");

    // Empty parts should be skipped
    EXPECT_EQ(result.size(), 2);
}
