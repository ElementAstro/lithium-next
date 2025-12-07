/**
 * @file debug.hpp
 * @brief Aggregated header for debug components.
 *
 * This is the primary include file for the debug module.
 * Include this file to get access to all debugging utilities
 * including core dump analysis, dynamic library parsing, and ELF parsing.
 *
 * @date 2024-6-4
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2024 Max Qian
 */

#ifndef LITHIUM_COMPONENTS_DEBUG_HPP
#define LITHIUM_COMPONENTS_DEBUG_HPP

#include <memory>
#include <string>

// ============================================================================
// Debug Components
// ============================================================================

#include "dump.hpp"
#include "dynamic.hpp"

#ifdef __linux__
#include "elf.hpp"
#endif

namespace lithium::addon {

// ============================================================================
// Module Version
// ============================================================================

/**
 * @brief Debug module version.
 */
inline constexpr const char* DEBUG_MODULE_VERSION = "1.1.0";

/**
 * @brief Get debug module version string.
 * @return Version string.
 */
[[nodiscard]] inline const char* getDebugModuleVersion() noexcept {
    return DEBUG_MODULE_VERSION;
}

// ============================================================================
// Convenience Type Aliases
// ============================================================================

/// Shared pointer to CoreDumpAnalyzer
using CoreDumpAnalyzerPtr = std::shared_ptr<CoreDumpAnalyzer>;

/// Shared pointer to DynamicLibraryParser
using DynamicLibraryParserPtr = std::shared_ptr<DynamicLibraryParser>;

#ifdef __linux__
/// Shared pointer to ElfParser (Linux only)
using ElfParserPtr = std::shared_ptr<lithium::ElfParser>;
#endif

// ============================================================================
// Factory Functions
// ============================================================================

/**
 * @brief Create a new CoreDumpAnalyzer instance.
 * @return Shared pointer to the new CoreDumpAnalyzer.
 */
[[nodiscard]] inline CoreDumpAnalyzerPtr createCoreDumpAnalyzer() {
    return std::make_shared<CoreDumpAnalyzer>();
}

/**
 * @brief Create a new DynamicLibraryParser instance.
 * @param executable Path to the executable to parse.
 * @return Shared pointer to the new DynamicLibraryParser.
 */
[[nodiscard]] inline DynamicLibraryParserPtr createDynamicLibraryParser(
    const std::string& executable) {
    return std::make_shared<DynamicLibraryParser>(executable);
}

#ifdef __linux__
/**
 * @brief Create a new ElfParser instance (Linux only).
 * @param file Path to the ELF file to parse.
 * @return Shared pointer to the new ElfParser.
 */
[[nodiscard]] inline ElfParserPtr createElfParser(const std::string& file) {
    return std::make_shared<lithium::ElfParser>(file);
}
#endif

// ============================================================================
// Quick Access Functions
// ============================================================================

/**
 * @brief Analyze a core dump file and generate a report.
 * @param filename Path to the core dump file.
 * @return Analysis report string, or empty string on failure.
 */
[[nodiscard]] inline std::string analyzeCoreDump(const std::string& filename) {
    auto analyzer = createCoreDumpAnalyzer();
    if (analyzer->readFile(filename)) {
        analyzer->analyze();
        return analyzer->generateReport();
    }
    return "";
}

/**
 * @brief Get dependencies of an executable.
 * @param executable Path to the executable.
 * @return Vector of dependency library names.
 */
[[nodiscard]] inline std::vector<std::string> getExecutableDependencies(
    const std::string& executable) {
    auto parser = createDynamicLibraryParser(executable);
    parser->parse();
    return parser->getDependencies();
}

/**
 * @brief Create a default parser configuration.
 * @return Default ParserConfig structure.
 */
[[nodiscard]] inline ParserConfig createDefaultParserConfig() {
    return ParserConfig{};
}

/**
 * @brief Create a parser configuration with JSON output enabled.
 * @param outputFilename The output filename for JSON.
 * @return ParserConfig with JSON output enabled.
 */
[[nodiscard]] inline ParserConfig createJsonParserConfig(
    const std::string& outputFilename) {
    ParserConfig config;
    config.json_output = true;
    config.output_filename = outputFilename;
    return config;
}

#ifdef __linux__
/**
 * @brief Check if a file is a valid ELF file (Linux only).
 * @param file Path to the file to check.
 * @return True if the file is a valid ELF file.
 */
[[nodiscard]] inline bool isValidElfFile(const std::string& file) {
    auto parser = createElfParser(file);
    return parser->parse();
}

/**
 * @brief Get the dependencies of an ELF file (Linux only).
 * @param file Path to the ELF file.
 * @return Vector of dependency library names.
 */
[[nodiscard]] inline std::vector<std::string> getElfDependencies(
    const std::string& file) {
    auto parser = createElfParser(file);
    if (parser->parse()) {
        return parser->getDependencies();
    }
    return {};
}
#endif

}  // namespace lithium::addon

#endif  // LITHIUM_COMPONENTS_DEBUG_HPP
