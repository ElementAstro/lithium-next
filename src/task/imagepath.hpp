#ifndef LITHIUM_TASK_IMAGEPATH_HPP
#define LITHIUM_TASK_IMAGEPATH_HPP

#include <atomic>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <string>
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
     * @brief Get performance statistics of the parser.
     * @return JSON object containing performance statistics.
     */
    [[nodiscard]] auto getPerformanceStats() const -> json;

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

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;  ///< Pointer to implementation.
    mutable std::shared_ptr<
        atom::search::ThreadSafeLRUCache<std::string, ImageInfo>>
        cache_;                                  ///< Cache for parsed results.
    mutable std::atomic<size_t> parseCount_{0};  ///< Count of parsed filenames.
    mutable std::atomic<size_t> cacheHits_{0};   ///< Count of cache hits.
    mutable std::string lastError_;              ///< Last error message.
    std::function<void(const std::string&)>
        errorHandler_;  ///< Custom error handler.
};

}  // namespace lithium

#endif  // LITHIUM_TASK_IMAGEPATH_HPP