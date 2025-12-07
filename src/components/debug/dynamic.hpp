#ifndef LITHIUM_ADDON_DEBUG_DYNAMIC_HPP
#define LITHIUM_ADDON_DEBUG_DYNAMIC_HPP

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace lithium::addon {

/**
 * @brief Configuration structure for the DynamicLibraryParser.
 */
struct ParserConfig {
    bool json_output{false};           ///< Flag to enable JSON output.
    bool use_cache{true};              ///< Flag to enable caching.
    bool verify_libraries{true};       ///< Flag to enable library verification.
    bool analyze_dependencies{false};  ///< Flag to enable dependency analysis.
    std::string cache_dir{".cache"};   ///< Directory for cache storage.
    std::string output_filename;       ///< Filename for output.
};

/**
 * @brief Class for parsing dynamic libraries of an executable.
 */
class DynamicLibraryParser {
public:
    /**
     * @brief Constructs a DynamicLibraryParser for the given executable.
     * @param executable Path to the executable to be parsed.
     */
    explicit DynamicLibraryParser(const std::string& executable);

    /**
     * @brief Destructor for DynamicLibraryParser.
     */
    ~DynamicLibraryParser();

    /**
     * @brief Sets the configuration for the parser.
     * @param config Configuration settings.
     */
    void setConfig(const ParserConfig& config);

    /**
     * @brief Parses the dynamic libraries of the executable.
     */
    void parse();

    /**
     * @brief Parses the dynamic libraries asynchronously.
     * @param callback Callback function to be called upon completion.
     */
    void parseAsync(std::function<void(bool)> callback);

    /**
     * @brief Gets the list of dependencies.
     * @return Vector of dependency library names.
     */
    [[nodiscard]] auto getDependencies() const -> std::vector<std::string>;

    /**
     * @brief Verifies if the given library path is valid.
     * @param library_path Path to the library to be verified.
     * @return True if the library is valid, false otherwise.
     */
    [[nodiscard]] auto verifyLibrary(const std::string& library_path) const
        -> bool;

    /**
     * @brief Clears the cache.
     */
    void clearCache();

    /**
     * @brief Sets the JSON output flag.
     * @param json_output True to enable JSON output, false to disable.
     */
    void setJsonOutput(bool json_output);

    /**
     * @brief Sets the output filename.
     * @param filename Name of the output file.
     */
    void setOutputFilename(const std::string& filename);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;  ///< Pointer to the implementation.
};

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * @brief Create a parser configuration for dependency analysis.
 * @return ParserConfig with dependency analysis enabled.
 */
[[nodiscard]] inline ParserConfig createDependencyAnalysisConfig() {
    ParserConfig config;
    config.analyze_dependencies = true;
    config.verify_libraries = true;
    return config;
}

/**
 * @brief Create a parser configuration with caching disabled.
 * @return ParserConfig with caching disabled.
 */
[[nodiscard]] inline ParserConfig createNoCacheConfig() {
    ParserConfig config;
    config.use_cache = false;
    return config;
}

/**
 * @brief Create a parser configuration for JSON output.
 * @param filename Output filename.
 * @return ParserConfig with JSON output enabled.
 */
[[nodiscard]] inline ParserConfig createJsonOutputConfig(
    const std::string& filename) {
    ParserConfig config;
    config.json_output = true;
    config.output_filename = filename;
    return config;
}

}  // namespace lithium::addon

#endif  // LITHIUM_ADDON_DEBUG_DYNAMIC_HPP
