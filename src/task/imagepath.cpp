#include "imagepath.hpp"

#include <filesystem>
#include <regex>
#include <unordered_map>

#include "atom/log/loguru.hpp"
#include "atom/type/json.hpp"

namespace fs = std::filesystem;

namespace lithium {

auto ImageInfo::toJson() const -> json {
    return {{"path", path},
            {"dateTime", dateTime.value_or("")},
            {"imageType", imageType.value_or("")},
            {"filter", filter.value_or("")},
            {"sensorTemp", sensorTemp.value_or("")},
            {"exposureTime", exposureTime.value_or("")},
            {"frameNr", frameNr.value_or("")}};
}

auto ImageInfo::fromJson(const json& jsonObj) -> ImageInfo {
    ImageInfo info;
    try {
        info.path = jsonObj.at("path").get<std::string>();
        info.dateTime = jsonObj.value("dateTime", "");
        info.imageType = jsonObj.value("imageType", "");
        info.filter = jsonObj.value("filter", "");
        info.sensorTemp = jsonObj.value("sensorTemp", "");
        info.exposureTime = jsonObj.value("exposureTime", "");
        info.frameNr = jsonObj.value("frameNr", "");
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Error deserializing ImageInfo from JSON: {}", e.what());
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
    if (!dateTime && other.dateTime) dateTime = other.dateTime;
    if (!imageType && other.imageType) imageType = other.imageType;
    if (!filter && other.filter) filter = other.filter;
    if (!sensorTemp && other.sensorTemp) sensorTemp = other.sensorTemp;
    if (!exposureTime && other.exposureTime) exposureTime = other.exposureTime;
    if (!frameNr && other.frameNr) frameNr = other.frameNr;
    if (!cameraModel && other.cameraModel) cameraModel = other.cameraModel;
    if (!gain && other.gain) gain = other.gain;
    if (!focalLength && other.focalLength) focalLength = other.focalLength;
    if (!target && other.target) target = other.target;
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
            LOG_F(INFO, "Cache insert: {}", key);
        });
    }

    void disableCache() { cache_.reset(); }

    [[nodiscard]] auto parseFilenames(const std::vector<std::string>& filenames)
        const -> std::vector<std::optional<ImageInfo>> {
        std::vector<std::optional<ImageInfo>> results;
        results.reserve(filenames.size());

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

    auto setFieldValidator(const std::string& field,
        std::function<bool(const std::string&)> validator) -> void {
        fieldValidators_[field] = std::move(validator);
    }
    
    auto setPreProcessor(std::function<std::string(std::string)> processor) -> void {
        preProcessor_ = std::move(processor);
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
    std::unordered_map<std::string, std::function<bool(const std::string&)>> fieldValidators_;
    std::function<std::string(std::string)> preProcessor_;
    mutable std::shared_mutex cacheMutex_;

    void parsePattern(const std::string& pattern) {
        static const std::regex TOKEN_REGEX(R"(\$(\w+))");
        std::smatch match;
        std::string regexPattern;
        std::string::const_iterator searchStart(pattern.cbegin());

        while (std::regex_search(searchStart, pattern.cend(), match,
                                 TOKEN_REGEX)) {
            regexPattern += std::regex_replace(
                match.prefix().str(),
                std::regex(R"([\.\+\*\?\^\$\(\)\[\]\{\}\\\|])"), "\\$&");

            std::string key = match[1];
            fieldKeys_.push_back(key);

            std::string fieldPattern = R"(.*)";  // 默认匹配任意内容

            if (auto it = fieldPatterns_.find(key);
                it != fieldPatterns_.end()) {
                fieldPattern = it->second;
            } else {
                // 默认模式，可根据需要调整
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
        regexPattern += std::regex_replace(
            std::string(searchStart, pattern.cend()),
            std::regex(R"([\.\+\*\?\^\$\(\)\[\]\{\}\\\|])"), "\\$&");

        patterns_.push_back(regexPattern);

        fullRegexPattern_ = std::regex(regexPattern);

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

        // 设置可选字段的默认值
        for (const auto& [key, defaultValue] : optionalFields_) {
            parsers_[key] = [defaultValue, key](ImageInfo& info,
                                                const std::string& value) {
                if (value.empty()) {
                    // 字段缺失，设置为默认值
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
                } else {
                    // 字段存在，使用实际值
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
                }
            };
        }
    }

    [[nodiscard]] auto parseFilenameImpl(const std::string& filename) const
        -> std::optional<ImageInfo> {
        using namespace std::views;
        using namespace std::ranges;
        
        std::shared_lock lock(cacheMutex_);
        if (cache_) {
            if (auto cached = cache_->get(filename)) {
                return cached;
            }
        }
        lock.unlock();
        
        std::smatch matchResult;
        if (!std::regex_match(filename, matchResult, fullRegexPattern_)) {
            LOG_F(ERROR, "Filename does not match the pattern: {}", filename);
            return std::nullopt;
        }

        ImageInfo info;
        info.path = fs::absolute(fs::path(filename)).string();

        auto processField = [this, &info](const auto& key, const auto& value) {
            if (auto validator = fieldValidators_.find(key);
                validator != fieldValidators_.end() && !validator->second(value)) {
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

        std::unique_lock writeLock(cacheMutex_);
        if (cache_) {
            cache_->put(filename, info);
        }
        
        return info;
    }

    template<typename T>
    [[nodiscard]] auto findFilesInDirectoryImpl(const std::filesystem::path& dir, T&& filter) const
        -> std::vector<ImageInfo> {
        std::vector<ImageInfo> results;
        
        for (const auto& entry : std::filesystem::recursive_directory_iterator(dir)) {
            if (!entry.is_regular_file()) continue;
            
            if (auto info = parseFilename(entry.path().string())) {
                if (filter(*info)) {
                    results.push_back(std::move(*info));
                }
            }
        }
        
        return results;
    }
};

ImagePatternParser::ImagePatternParser(const std::string& pattern)
    : pImpl(std::make_unique<Impl>(pattern)) {}

ImagePatternParser::~ImagePatternParser() = default;

ImagePatternParser::ImagePatternParser(ImagePatternParser&& other) noexcept
    : pImpl(std::move(other.pImpl)) {}

auto ImagePatternParser::operator=(ImagePatternParser&& other) noexcept
    -> ImagePatternParser& {
    if (this != &other) {
        pImpl = std::move(other.pImpl);
    }
    return *this;
}

auto ImagePatternParser::parseFilename(const std::string& filename) const
    -> std::optional<ImageInfo> {
    return pImpl->parseFilename(filename);
}

auto ImagePatternParser::serializeToJson(const ImageInfo& info) -> json {
    return info.toJson();
}

auto ImagePatternParser::deserializeFromJson(const json& jsonObj) -> ImageInfo {
    return ImageInfo::fromJson(jsonObj);
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
    return pImpl->getPatterns();
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

}  // namespace lithium
