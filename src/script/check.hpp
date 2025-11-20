#ifndef LITHIUM_SCRIPT_CHECKER_HPP
#define LITHIUM_SCRIPT_CHECKER_HPP

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "atom/error/exception.hpp"
#include "atom/type/noncopyable.hpp"

/**
 * @brief Exception class for invalid format errors.
 */
class InvalidFormatException : public atom::error::Exception {
public:
    using Exception::Exception;
};

#define THROW_INVALID_FORMAT(...)                                \
    throw InvalidFormatException(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                 ATOM_FUNC_NAME, __VA_ARGS__);

namespace lithium {
class ScriptAnalyzerImpl;

/**
 * @brief Enum representing the report format.
 */
enum class ReportFormat { TEXT, JSON, XML };

/**
 * @brief Structure representing the analyzer options.
 */
struct AnalyzerOptions {
    bool async_mode{true};      ///< Whether to use asynchronous analysis.
    bool deep_analysis{false};  ///< Whether to perform deep analysis.
    size_t thread_count{4};     ///< Number of analysis threads.
    int timeout_seconds{30};    ///< Analysis timeout in seconds.
    std::vector<std::string>
        ignore_patterns;  ///< Patterns to ignore during analysis.
};

/**
 * @brief Structure representing a danger item found during analysis.
 */
struct DangerItem {
    std::string category;  ///< Category of the danger item.
    std::string command;   ///< Command that caused the danger item.
    std::string reason;    ///< Reason for the danger item.
    int line;              ///< Line number where the danger item was found.
    std::optional<std::string>
        context;  ///< Optional context of the danger item.
};

/**
 * @brief Structure representing the result of an analysis.
 */
struct AnalysisResult {
    int complexity;  ///< Complexity of the analyzed script.
    std::vector<DangerItem>
        dangers;            ///< List of danger items found during analysis.
    double execution_time;  ///< Execution time of the analysis.
    bool timeout_occurred;  ///< Whether a timeout occurred during analysis.
};

/**
 * @brief Class for analyzing scripts.
 */
class ScriptAnalyzer : public NonCopyable {
public:
    /**
     * @brief Constructs a ScriptAnalyzer instance.
     *
     * @param config_file Path to the configuration file.
     */
    explicit ScriptAnalyzer(const std::string& config_file);

    /**
     * @brief Destructs the ScriptAnalyzer instance.
     */
    ~ScriptAnalyzer() override;

    /**
     * @brief Analyzes a script.
     *
     * @param script The script to analyze.
     * @param output_json Whether to output the result in JSON format.
     * @param format The format of the report.
     */
    void analyze(const std::string& script, bool output_json = false,
                 ReportFormat format = ReportFormat::TEXT);

    /**
     * @brief Analyzes a script with specified options.
     *
     * @param script The script to analyze.
     * @param options The options for the analysis.
     * @return The result of the analysis.
     */
    AnalysisResult analyzeWithOptions(const std::string& script,
                                      const AnalyzerOptions& options);

    /**
     * @brief Updates the configuration file.
     *
     * @param config_file Path to the new configuration file.
     */
    void updateConfig(const std::string& config_file);

    /**
     * @brief Adds a custom pattern to the analyzer.
     *
     * @param pattern The pattern to add.
     * @param category The category of the pattern.
     */
    void addCustomPattern(const std::string& pattern,
                          const std::string& category);

    /**
     * @brief Sets a callback function to be called when a danger item is found.
     *
     * @param callback The callback function.
     */
    void setCallback(std::function<void(const DangerItem&)> callback);

    /**
     * @brief Validates a script.
     *
     * @param script The script to validate.
     * @return True if the script is valid, false otherwise.
     */
    bool validateScript(const std::string& script);

    /**
     * @brief Gets a safe version of a script.
     *
     * @param script The script to sanitize.
     * @return The safe version of the script.
     */
    std::string getSafeVersion(const std::string& script);

    /**
     * @brief Gets the total number of analyzed scripts.
     *
     * @return The total number of analyzed scripts.
     */
    size_t getTotalAnalyzed() const;

    /**
     * @brief Gets the average analysis time.
     *
     * @return The average analysis time.
     */
    double getAverageAnalysisTime() const;

private:
    std::unique_ptr<ScriptAnalyzerImpl>
        impl_;  ///< Pointer to the implementation class.
};
}  // namespace lithium

#endif  // LITHIUM_SCRIPT_CHECKER_HPP