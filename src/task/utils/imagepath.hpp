#ifndef LITHIUM_TASK_UTILS_IMAGEPATH_HPP
#define LITHIUM_TASK_UTILS_IMAGEPATH_HPP

#include <atomic>
#include <filesystem>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "atom/macro.hpp"
#include "atom/search/lru.hpp"
#include "atom/type/json_fwd.hpp"

using json = nlohmann::json;

namespace lithium {

/**
 * @brief Struct representing image information.
 */
struct ImageInfo {
    std::string path;  ///< Path to the image file.
    std::optional<std::string>
        dateTime;  ///< Date and time when the image was taken.
    std::optional<std::string> imageType;  ///< Type of the image.
    std::optional<std::string> filter;     ///< Filter used for the image.
    std::optional<std::string>
        sensorTemp;  ///< Sensor temperature when the image was taken.
    std::optional<std::string> exposureTime;  ///< Exposure time of the image.
    std::optional<std::string> frameNr;       ///< Frame number of the image.
    std::optional<std::string> cameraModel;   ///< Camera model
    std::optional<uint32_t> gain;             ///< Gain value
    std::optional<double> focalLength;        ///< Focal length
    std::optional<std::string> target;        ///< Shooting target

    /**
     * @brief Convert the ImageInfo to a JSON object.
     * @return JSON representation of the ImageInfo.
     */
    [[nodiscard]] auto toJson() const -> json;

    /**
     * @brief Create an ImageInfo from a JSON object.
     * @param j JSON object to convert.
     * @return ImageInfo created from the JSON object.
     */
    static auto fromJson(const json& j) -> ImageInfo;

    bool operator==(const ImageInfo&) const = default;

    /**
     * @brief Compute a hash value for the ImageInfo.
     * @return Hash value.
     */
    [[nodiscard]] auto hash() const -> size_t;

    /**
     * @brief Check if the ImageInfo is complete.
     * @return True if complete, false otherwise.
     */
    [[nodiscard]] auto isComplete() const noexcept -> bool;

    /**
     * @brief Merge with another ImageInfo.
     * @param other The other ImageInfo to merge with.
     */
    auto mergeWith(const ImageInfo& other) -> void;
} ATOM_ALIGNAS(128);

/**
 * @brief Class for parsing image file patterns.
 */
class ImagePatternParser {
public:
    using FieldParser = std::function<void(ImageInfo&, const std::string&)>;

    /**
     * @brief Construct an ImagePatternParser with a given pattern.
     * @param pattern The pattern to use for parsing.
     */
    explicit ImagePatternParser(const std::string& pattern);

    /**
     * @brief Destructor for ImagePatternParser.
     */
    ~ImagePatternParser();

    // Disable copy operations
    ImagePatternParser(const ImagePatternParser&) = delete;
    ImagePatternParser& operator=(const ImagePatternParser&) = delete;

    // Enable move operations
    ImagePatternParser(ImagePatternParser&&) noexcept;
    ImagePatternParser& operator=(ImagePatternParser&&) noexcept;

    /**
     * @brief Parse a filename to extract image information.
     * @param filename The filename to parse.
     * @return Optional ImageInfo if parsing is successful.
     */
    [[nodiscard]] auto parseFilename(const std::string& filename) const
        -> std::optional<ImageInfo>;

    /**
     * @brief Parse multiple filenames to extract image information.
     * @param filenames The filenames to parse.
     * @return Vector of optional ImageInfo for each filename.
     */
    [[nodiscard]] auto parseFilenames(const std::vector<std::string>& filenames)
        const -> std::vector<std::optional<ImageInfo>>;

    /**
     * @brief Asynchronously parse a filename to extract image information.
     * @param filename The filename to parse.
     * @return Future holding optional ImageInfo if parsing is successful.
     */
    [[nodiscard]] auto parseFilenameAsync(const std::string& filename)
        -> std::future<std::optional<ImageInfo>>;

    /**
     * @brief Enable caching of parsed results.
     * @param maxSize Maximum size of the cache.
     */
    void enableCache(size_t maxSize = 1000);

    /**
     * @brief Disable caching of parsed results.
     */
    void disableCache();

    /**
     * @brief Clear the cache.
     */
    void clearCache();

    /**
     * @brief Get the last error message.
     * @return Last error message as a string.
     */
    [[nodiscard]] auto getLastError() const -> std::string;

    /**
     * @brief Set a custom error handler.
     * @param handler Function to handle errors.
     */
    void setErrorHandler(std::function<void(const std::string&)> handler);

    /**
     * @brief Validate a pattern.
     * @param pattern The pattern to validate.
     * @return True if the pattern is valid, false otherwise.
     */
    [[nodiscard]] bool validatePattern(const std::string& pattern) const;

    /**
     * @brief Serialize ImageInfo to JSON.
     * @param info The ImageInfo to serialize.
     * @return JSON representation of the ImageInfo.
     */
    static auto serializeToJson(const ImageInfo& info) -> json;

    /**
     * @brief Deserialize ImageInfo from JSON.
     * @param j The JSON object to deserialize.
     * @return ImageInfo created from the JSON object.
     */
    static auto deserializeFromJson(const json& j) -> ImageInfo;

    /**
     * @brief Add a custom parser for a specific field.
     * @param key The field key.
     * @param parser The parser function.
     */
    void addCustomParser(const std::string& key, FieldParser parser);

    /**
     * @brief Set a default value for an optional field.
     * @param key The field key.
     * @param defaultValue The default value.
     */
    void setOptionalField(const std::string& key,
                          const std::string& defaultValue);

    /**
     * @brief Add a regex pattern for a specific field.
     * @param key The field key.
     * @param regexPattern The regex pattern.
     */
    void addFieldPattern(const std::string& key,
                         const std::string& regexPattern);

    /**
     * @brief Get all field patterns.
     * @return Vector of field patterns.
     */
    [[nodiscard]] auto getPatterns() const -> std::vector<std::string>;

    /**
     * @brief Check if a filename is valid.
     * @param filename The filename to check.
     * @return True if valid, false otherwise.
     */
    [[nodiscard]] auto isValidFilename(std::string_view filename) const noexcept
        -> bool;

    /**
     * @brief Get a batch processor for processing files in batches.
     * @param batchSize The size of the batch.
     * @return Unique pointer to the batch processor.
     */
    [[nodiscard]] auto getBatchProcessor(size_t batchSize)
        -> std::unique_ptr<class BatchProcessor>;

    /**
     * @brief Find files in a directory.
     * @param dir The directory to search.
     * @param filter The filter function.
     * @return Vector of ImageInfo for the found files.
     */
    template <typename T>
    [[nodiscard]] auto findFilesInDirectory(
        const std::filesystem::path& dir, T&& filter = [](const auto&) {
            return true;
        }) const -> std::vector<ImageInfo>;

    /**
     * @brief Create a file namer based on a pattern.
     * @param pattern The pattern to use.
     * @return Function to generate filenames.
     */
    [[nodiscard]] auto createFileNamer(const std::string& pattern) const
        -> std::function<std::string(const ImageInfo&)>;

    /**
     * @brief Set a validator for a specific field.
     * @param field The field to validate.
     * @param validator The validator function.
     */
    auto setFieldValidator(const std::string& field,
                           std::function<bool(const std::string&)> validator)
        -> void;

    /**
     * @brief Set a pre-processor for filenames.
     * @param processor The pre-processor function.
     */
    auto setPreProcessor(std::function<std::string(std::string)> processor)
        -> void;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;  ///< Pointer to implementation.
    std::shared_ptr<atom::search::ThreadSafeLRUCache<std::string, ImageInfo>>
        cache_;                                  ///< Cache for parsed results.
    mutable std::atomic<size_t> parseCount_{0};  ///< Count of parsed filenames.
    mutable std::atomic<size_t> cacheHits_{0};   ///< Count of cache hits.
    mutable std::string lastError_;              ///< Last error message.
    std::function<void(const std::string&)>
        errorHandler_;  ///< Custom error handler.
    mutable std::shared_mutex cacheMutex_;
    std::unordered_map<std::string, std::function<bool(const std::string&)>>
        fieldValidators_;
    std::function<std::string(std::string)> preProcessor_;
};

}  // namespace lithium

#endif  // LITHIUM_TASK_UTILS_IMAGEPATH_HPP
