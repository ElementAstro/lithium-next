#include "imagepath.hpp"

#include <filesystem>
#include <regex>
#include <unordered_map>

#include "atom/type/json.hpp"
#include "spdlog/spdlog.h"

namespace fs = std::filesystem;

namespace lithium {

auto ImageInfo::toJson() const -> json {
    return {{"path", path},
            {"dateTime", dateTime.value_or("")},
            {"imageType", imageType.value_or("")},
            {"filter", filter.value_or("")},
            {"sensorTemp", sensorTemp.value_or("")},
            {"exposureTime", exposureTime.value_or("")},
            {"frameNr", frameNr.value_or("")},
            {"cameraModel", cameraModel.value_or("")},
            {"gain", gain ? json(*gain) : json("")},
            {"focalLength", focalLength ? json(*focalLength) : json("")},
            {"target", target.value_or("")}};
}

auto ImageInfo::fromJson(const json& jsonObj) -> ImageInfo {
    ImageInfo info;
    try {
        info.path = jsonObj.at("path").get<std::string>();

        if (jsonObj.contains("dateTime") && !jsonObj["dateTime"].is_null())
            info.dateTime = jsonObj["dateTime"].get<std::string>();

        if (jsonObj.contains("imageType") && !jsonObj["imageType"].is_null())
            info.imageType = jsonObj["imageType"].get<std::string>();

        if (jsonObj.contains("filter") && !jsonObj["filter"].is_null())
            info.filter = jsonObj["filter"].get<std::string>();

        if (jsonObj.contains("sensorTemp") && !jsonObj["sensorTemp"].is_null())
            info.sensorTemp = jsonObj["sensorTemp"].get<std::string>();

        if (jsonObj.contains("exposureTime") &&
            !jsonObj["exposureTime"].is_null())
            info.exposureTime = jsonObj["exposureTime"].get<std::string>();

        if (jsonObj.contains("frameNr") && !jsonObj["frameNr"].is_null())
            info.frameNr = jsonObj["frameNr"].get<std::string>();

        if (jsonObj.contains("cameraModel") &&
            !jsonObj["cameraModel"].is_null())
            info.cameraModel = jsonObj["cameraModel"].get<std::string>();

        if (jsonObj.contains("gain") && jsonObj["gain"].is_number_unsigned())
            info.gain = jsonObj["gain"].get<uint32_t>();

        if (jsonObj.contains("focalLength") &&
            jsonObj["focalLength"].is_number())
            info.focalLength = jsonObj["focalLength"].get<double>();

        if (jsonObj.contains("target") && !jsonObj["target"].is_null())
            info.target = jsonObj["target"].get<std::string>();
    } catch (const std::exception& e) {
        spdlog::error("Error deserializing ImageInfo from JSON: {}", e.what());
    }
    return info;
}

auto ImageInfo::hash() const -> size_t {
    return std::hash<std::string>{}(path);
}

auto ImageInfo::isComplete() const noexcept -> bool {
    return dateTime.has_value() && imageType.has_value() &&
           filter.has_value() && exposureTime.has_value();
}

auto ImageInfo::mergeWith(const ImageInfo& other) -> void {
    if (!dateTime && other.dateTime)
        dateTime = other.dateTime;
    if (!imageType && other.imageType)
        imageType = other.imageType;
    if (!filter && other.filter)
        filter = other.filter;
    if (!sensorTemp && other.sensorTemp)
        sensorTemp = other.sensorTemp;
    if (!exposureTime && other.exposureTime)
        exposureTime = other.exposureTime;
    if (!frameNr && other.frameNr)
        frameNr = other.frameNr;
    if (!cameraModel && other.cameraModel)
        cameraModel = other.cameraModel;
    if (!gain && other.gain)
        gain = other.gain;
    if (!focalLength && other.focalLength)
        focalLength = other.focalLength;
    if (!target && other.target)
        target = other.target;
}

class ImagePatternParser::Impl {
public:
    explicit Impl(const std::string& pattern) { parsePattern(pattern); }

    [[nodiscard]] auto parseFilename(const std::string& filename) const
        -> std::optional<ImageInfo> {
        if (preProcessor_) {
            auto processed = preProcessor_(filename);
            return parseFilenameImpl(processed);
        }
        return parseFilenameImpl(filename);
    }

    void addCustomParser(const std::string& key, FieldParser parser) {
        parsers_[key] = std::move(parser);
    }

    void setOptionalField(const std::string& key,
                          const std::string& defaultValue) {
        optionalFields_[key] = defaultValue;
    }

    void addFieldPattern(const std::string& key,
                         const std::string& regexPattern) {
        fieldPatterns_[key] = regexPattern;
    }

    [[nodiscard]] auto getPatterns() const -> const std::vector<std::string>& {
        return patterns_;
    }

    void enableCache(size_t maxSize) {
        cache_ = std::make_shared<
            atom::search::ThreadSafeLRUCache<std::string, ImageInfo>>(maxSize);
        cache_->setInsertCallback([](const std::string& key, const ImageInfo&) {
            spdlog::info("Cache insert: {}", key);
        });
    }

    void disableCache() { cache_.reset(); }

    void clearCache() {
        if (cache_) {
            cache_->clear();
            spdlog::info("Cache cleared");
        }
    }

    [[nodiscard]] auto parseFilenames(const std::vector<std::string>& filenames)
        const -> std::vector<std::optional<ImageInfo>> {
        std::vector<std::optional<ImageInfo>> results(filenames.size());

#pragma omp parallel for if (filenames.size() > 100)
        for (size_t i = 0; i < filenames.size(); ++i) {
            results[i] = parseFilename(filenames[i]);
        }

        return results;
    }

    [[nodiscard]] auto parseFilenameAsync(const std::string& filename)
        -> std::future<std::optional<ImageInfo>> {
        return std::async(std::launch::async, [this, filename]() {
            return parseFilename(filename);
        });
    }

    [[nodiscard]] auto getPerformanceStats() const -> json {
        return {{"parseCount", parseCount_.load()},
                {"cacheHits", cacheHits_.load()},
                {"cacheHitRate", cache_ ? cache_->hitRate() : 0.0f}};
    }

    [[nodiscard]] auto getLastError() const -> const std::string& {
        return lastError_;
    }

    void setErrorHandler(std::function<void(const std::string&)> handler) {
        errorHandler_ = std::move(handler);
    }

    [[nodiscard]] bool validatePattern(const std::string& pattern) const {
        try {
            std::regex testRegex(pattern);
            return true;
        } catch (const std::regex_error& e) {
            if (errorHandler_) {
                errorHandler_(e.what());
            } else {
                spdlog::error("Invalid regex pattern: {}", e.what());
            }
            return false;
        }
    }

    auto setFieldValidator(const std::string& field,
                           std::function<bool(const std::string&)> validator)
        -> void {
        fieldValidators_[field] = std::move(validator);
    }

    auto setPreProcessor(std::function<std::string(std::string)> processor)
        -> void {
        preProcessor_ = std::move(processor);
    }

    [[nodiscard]] auto isValidFilename(std::string_view filename) const noexcept
        -> bool {
        std::string filenameStr{filename};
        std::smatch matchResult;
        return std::regex_match(filenameStr, matchResult, fullRegexPattern_);
    }

    [[nodiscard]] auto createFileNamer(const std::string& pattern) const
        -> std::function<std::string(const ImageInfo&)> {
        return [pattern](const ImageInfo& info) -> std::string {
            std::string result = pattern;

            if (info.dateTime) {
                result = std::regex_replace(result, std::regex("\\$DATETIME"),
                                            *info.dateTime);
            }
            if (info.imageType) {
                result = std::regex_replace(result, std::regex("\\$IMAGETYPE"),
                                            *info.imageType);
            }
            if (info.filter) {
                result = std::regex_replace(result, std::regex("\\$FILTER"),
                                            *info.filter);
            }
            if (info.sensorTemp) {
                result = std::regex_replace(result, std::regex("\\$SENSORTEMP"),
                                            *info.sensorTemp);
            }
            if (info.exposureTime) {
                result = std::regex_replace(
                    result, std::regex("\\$EXPOSURETIME"), *info.exposureTime);
            }
            if (info.frameNr) {
                result = std::regex_replace(result, std::regex("\\$FRAMENR"),
                                            *info.frameNr);
            }
            if (info.cameraModel) {
                result = std::regex_replace(
                    result, std::regex("\\$CAMERAMODEL"), *info.cameraModel);
            }
            if (info.gain) {
                result = std::regex_replace(result, std::regex("\\$GAIN"),
                                            std::to_string(*info.gain));
            }
            if (info.focalLength) {
                result =
                    std::regex_replace(result, std::regex("\\$FOCALLENGTH"),
                                       std::to_string(*info.focalLength));
            }
            if (info.target) {
                result = std::regex_replace(result, std::regex("\\$TARGET"),
                                            *info.target);
            }

            return result;
        };
    }

private:
    std::vector<std::string> fieldKeys_;
    std::vector<std::string> patterns_;
    std::unordered_map<std::string, FieldParser> parsers_;
    std::unordered_map<std::string, std::string> optionalFields_;
    std::unordered_map<std::string, std::string> fieldPatterns_;
    std::regex fullRegexPattern_;
    std::shared_ptr<atom::search::ThreadSafeLRUCache<std::string, ImageInfo>>
        cache_;
    mutable std::atomic<size_t> parseCount_{0};
    mutable std::atomic<size_t> cacheHits_{0};
    mutable std::string lastError_;
    std::function<void(const std::string&)> errorHandler_;
    std::unordered_map<std::string, std::function<bool(const std::string&)>>
        fieldValidators_;
    std::function<std::string(std::string)> preProcessor_;
    mutable std::shared_mutex cacheMutex_;

    void parsePattern(const std::string& pattern) {
        static const std::regex TOKEN_REGEX(R"(\$(\w+))");
        std::smatch match;
        std::string regexPattern;
        std::string::const_iterator searchStart(pattern.cbegin());

        while (std::regex_search(searchStart, pattern.cend(), match,
                                 TOKEN_REGEX)) {
            regexPattern += std::regex_replace(match.prefix().str(),
                                               std::regex(R"([\.\+\*\?\^\$$$$$

$$\{\}\\\|])"),
                                               "\\$&");

            std::string key = match[1];
            fieldKeys_.push_back(key);

            std::string fieldPattern =
                R"(.*)";  // Default pattern to match any content

            if (auto it = fieldPatterns_.find(key);
                it != fieldPatterns_.end()) {
                fieldPattern = it->second;
            } else {
                // Default patterns based on field type
                if (key == "DATETIME") {
                    fieldPattern = R"(\d{4}-\d{2}-\d{2}-\d{2}-\d{2}-\d{2})";
                } else if (key == "EXPOSURETIME") {
                    fieldPattern = R"(\d+(\.\d+)?)";
                } else {
                    fieldPattern = R"(\w+)";
                }
            }

            regexPattern += "(" + fieldPattern + ")";
            searchStart = match.suffix().first;
        }
        regexPattern +=
            std::regex_replace(std::string(searchStart, pattern.cend()),
                               std::regex(R"([\.\+\*\?\^\$$$$$

$$\{\}\\\|])"),
                               "\\$&");

        patterns_.push_back(regexPattern);

        try {
            fullRegexPattern_ = std::regex(regexPattern);
        } catch (const std::regex_error& e) {
            lastError_ = "Invalid regex pattern: " + std::string(e.what());
            spdlog::error(lastError_);
            throw std::invalid_argument(lastError_);
        }

        initializeParsers();
    }

    void initializeParsers() {
        parsers_["DATETIME"] = [](ImageInfo& info, const std::string& value) {
            info.dateTime = value;
        };
        parsers_["IMAGETYPE"] = [](ImageInfo& info, const std::string& value) {
            info.imageType = value;
        };
        parsers_["FILTER"] = [](ImageInfo& info, const std::string& value) {
            info.filter = value;
        };
        parsers_["SENSORTEMP"] = [](ImageInfo& info, const std::string& value) {
            info.sensorTemp = value;
        };
        parsers_["EXPOSURETIME"] = [](ImageInfo& info,
                                      const std::string& value) {
            info.exposureTime = value;
        };
        parsers_["FRAMENR"] = [](ImageInfo& info, const std::string& value) {
            info.frameNr = value;
        };
        parsers_["CAMERAMODEL"] = [](ImageInfo& info,
                                     const std::string& value) {
            info.cameraModel = value;
        };
        parsers_["GAIN"] = [](ImageInfo& info, const std::string& value) {
            try {
                info.gain = static_cast<uint32_t>(std::stoul(value));
            } catch (const std::exception& e) {
                spdlog::warn("Failed to convert gain value '{}': {}", value,
                             e.what());
            }
        };
        parsers_["FOCALLENGTH"] = [](ImageInfo& info,
                                     const std::string& value) {
            try {
                info.focalLength = std::stod(value);
            } catch (const std::exception& e) {
                spdlog::warn("Failed to convert focal length value '{}': {}",
                             value, e.what());
            }
        };
        parsers_["TARGET"] = [](ImageInfo& info, const std::string& value) {
            info.target = value;
        };

        // Set default values for optional fields
        for (const auto& [key, defaultValue] : optionalFields_) {
            parsers_[key] = [defaultValue, key](ImageInfo& info,
                                                const std::string& value) {
                if (value.empty()) {
                    // Field is missing, set default value
                    if (key == "DATETIME")
                        info.dateTime = defaultValue;
                    else if (key == "IMAGETYPE")
                        info.imageType = defaultValue;
                    else if (key == "FILTER")
                        info.filter = defaultValue;
                    else if (key == "SENSORTEMP")
                        info.sensorTemp = defaultValue;
                    else if (key == "EXPOSURETIME")
                        info.exposureTime = defaultValue;
                    else if (key == "FRAMENR")
                        info.frameNr = defaultValue;
                    else if (key == "CAMERAMODEL")
                        info.cameraModel = defaultValue;
                    else if (key == "TARGET")
                        info.target = defaultValue;
                } else {
                    // Field exists, use actual value
                    if (key == "DATETIME")
                        info.dateTime = value;
                    else if (key == "IMAGETYPE")
                        info.imageType = value;
                    else if (key == "FILTER")
                        info.filter = value;
                    else if (key == "SENSORTEMP")
                        info.sensorTemp = value;
                    else if (key == "EXPOSURETIME")
                        info.exposureTime = value;
                    else if (key == "FRAMENR")
                        info.frameNr = value;
                    else if (key == "CAMERAMODEL")
                        info.cameraModel = value;
                    else if (key == "TARGET")
                        info.target = value;
                }
            };
        }
    }

    [[nodiscard]] auto parseFilenameImpl(const std::string& filename) const
        -> std::optional<ImageInfo> {
        parseCount_++;

        if (cache_) {
            std::shared_lock lock(cacheMutex_);
            if (auto cached = cache_->get(filename)) {
                cacheHits_++;
                return cached;
            }
        }

        std::smatch matchResult;
        if (!std::regex_match(filename, matchResult, fullRegexPattern_)) {
            spdlog::debug("Filename '{}' does not match pattern", filename);
            return std::nullopt;
        }

        ImageInfo info;
        info.path = fs::absolute(fs::path(filename)).string();

        auto processField = [this, &info](const auto& key, const auto& value) {
            if (auto validator = fieldValidators_.find(key);
                validator != fieldValidators_.end() &&
                !validator->second(value)) {
                spdlog::debug("Field '{}' with value '{}' failed validation",
                              key, value);
                return false;
            }

            if (auto parser = parsers_.find(key); parser != parsers_.end()) {
                parser->second(info, value);
            }
            return true;
        };

        for (size_t i = 0; i < fieldKeys_.size(); ++i) {
            if (!processField(fieldKeys_[i], matchResult[i + 1].str())) {
                return std::nullopt;
            }
        }

        if (cache_) {
            std::unique_lock writeLock(cacheMutex_);
            cache_->put(filename, info);
        }

        return info;
    }
};

ImagePatternParser::ImagePatternParser(const std::string& pattern)
    : pImpl(std::make_unique<Impl>(pattern)) {}

ImagePatternParser::~ImagePatternParser() = default;

ImagePatternParser::ImagePatternParser(ImagePatternParser&& other) noexcept
    : pImpl(std::move(other.pImpl)),
      cache_(std::move(other.cache_)),
      parseCount_(other.parseCount_.load()),
      cacheHits_(other.cacheHits_.load()),
      lastError_(std::move(other.lastError_)),
      errorHandler_(std::move(other.errorHandler_)),
      fieldValidators_(std::move(other.fieldValidators_)),
      preProcessor_(std::move(other.preProcessor_)) {}

auto ImagePatternParser::operator=(ImagePatternParser&& other) noexcept
    -> ImagePatternParser& {
    if (this != &other) {
        pImpl = std::move(other.pImpl);
        cache_ = std::move(other.cache_);
        parseCount_ = other.parseCount_.load();
        cacheHits_ = other.cacheHits_.load();
        lastError_ = std::move(other.lastError_);
        errorHandler_ = std::move(other.errorHandler_);
        fieldValidators_ = std::move(other.fieldValidators_);
        preProcessor_ = std::move(other.preProcessor_);
    }
    return *this;
}

auto ImagePatternParser::parseFilename(const std::string& filename) const
    -> std::optional<ImageInfo> {
    return pImpl->parseFilename(filename);
}

auto ImagePatternParser::parseFilenames(
    const std::vector<std::string>& filenames) const
    -> std::vector<std::optional<ImageInfo>> {
    return pImpl->parseFilenames(filenames);
}

auto ImagePatternParser::parseFilenameAsync(const std::string& filename)
    -> std::future<std::optional<ImageInfo>> {
    return pImpl->parseFilenameAsync(filename);
}

void ImagePatternParser::enableCache(size_t maxSize) {
    pImpl->enableCache(maxSize);
}

void ImagePatternParser::disableCache() { pImpl->disableCache(); }

void ImagePatternParser::clearCache() { pImpl->clearCache(); }

auto ImagePatternParser::getLastError() const -> std::string {
    return pImpl->getLastError();
}

void ImagePatternParser::setErrorHandler(
    std::function<void(const std::string&)> handler) {
    pImpl->setErrorHandler(std::move(handler));
}

bool ImagePatternParser::validatePattern(const std::string& pattern) const {
    return pImpl->validatePattern(pattern);
}

auto ImagePatternParser::serializeToJson(const ImageInfo& info) -> json {
    return info.toJson();
}

auto ImagePatternParser::deserializeFromJson(const json& j) -> ImageInfo {
    return ImageInfo::fromJson(j);
}

void ImagePatternParser::addCustomParser(const std::string& key,
                                         FieldParser parser) {
    pImpl->addCustomParser(key, std::move(parser));
}

void ImagePatternParser::setOptionalField(const std::string& key,
                                          const std::string& defaultValue) {
    pImpl->setOptionalField(key, defaultValue);
}

void ImagePatternParser::addFieldPattern(const std::string& key,
                                         const std::string& regexPattern) {
    pImpl->addFieldPattern(key, regexPattern);
}

auto ImagePatternParser::getPatterns() const -> std::vector<std::string> {
    return std::vector<std::string>(pImpl->getPatterns());
}

auto ImagePatternParser::isValidFilename(
    std::string_view filename) const noexcept -> bool {
    return pImpl->isValidFilename(filename);
}

auto ImagePatternParser::setFieldValidator(
    const std::string& field, std::function<bool(const std::string&)> validator)
    -> void {
    pImpl->setFieldValidator(field, std::move(validator));
}

auto ImagePatternParser::setPreProcessor(
    std::function<std::string(std::string)> processor) -> void {
    pImpl->setPreProcessor(std::move(processor));
}

auto ImagePatternParser::createFileNamer(const std::string& pattern) const
    -> std::function<std::string(const ImageInfo&)> {
    return pImpl->createFileNamer(pattern);
}

}  // namespace lithium