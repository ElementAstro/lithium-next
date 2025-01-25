#ifndef LITHIUM_ADDON_TRACKER_HPP
#define LITHIUM_ADDON_TRACKER_HPP

#include <chrono>
#include <concepts>
#include <filesystem>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "atom/macro.hpp"
#include "atom/type/json_fwd.hpp"
using json = nlohmann::json;

namespace fs = std::filesystem;

namespace lithium {

/**
 * @brief Class for tracking and managing files in a directory.
 */
class FileTracker {
public:
    /**
     * @brief Constructs a FileTracker object.
     * @param directory The directory to track.
     * @param jsonFilePath The path to the JSON file for storing file
     * information.
     * @param fileTypes The types of files to track.
     * @param recursive Whether to track files recursively in subdirectories.
     */
    FileTracker(std::string_view directory, std::string_view jsonFilePath,
                std::span<const std::string> fileTypes, bool recursive = false);

    /**
     * @brief Destructor for FileTracker.
     */
    ~FileTracker();

    FileTracker(const FileTracker&) = delete;
    auto operator=(const FileTracker&) -> FileTracker& = delete;
    FileTracker(FileTracker&&) noexcept;
    auto operator=(FileTracker&&) noexcept -> FileTracker&;

    /**
     * @brief Scans the directory and generates a JSON file with file
     * information.
     */
    void scan();

    /**
     * @brief Compares the current state of the directory with the previous
     * state.
     */
    void compare();

    /**
     * @brief Logs the differences between the current and previous states to a
     * file.
     * @param logFilePath The path to the log file.
     */
    void logDifferences(std::string_view logFilePath) const;

    /**
     * @brief Recovers files based on the information in a JSON file.
     * @param jsonFilePath The path to the JSON file.
     */
    void recover(std::string_view jsonFilePath);

    /**
     * @brief Asynchronously scans the directory.
     * @return A future object representing the asynchronous scan operation.
     */
    [[nodiscard]] auto asyncScan() -> std::future<void>;

    /**
     * @brief Asynchronously compares the current state of the directory with
     * the previous state.
     * @return A future object representing the asynchronous compare operation.
     */
    [[nodiscard]] auto asyncCompare() -> std::future<void>;

    /**
     * @brief Gets the differences between the current and previous states.
     * @return A JSON object containing the differences.
     */
    [[nodiscard]] auto getDifferences() const noexcept -> const json&;

    /**
     * @brief Gets the types of files being tracked.
     * @return A vector of strings containing the file types.
     */
    [[nodiscard]] auto getTrackedFileTypes() const noexcept
        -> const std::vector<std::string>&;

    /**
     * @brief Applies a function to each tracked file.
     * @tparam Func The type of the function.
     * @param func The function to apply to each file.
     */
    template <std::invocable<const fs::path&> Func>
    void forEachFile(Func&& func) const;

    /**
     * @brief Gets information about a specific file.
     * @param filePath The path to the file.
     * @return An optional JSON object containing the file information.
     */
    [[nodiscard]] auto getFileInfo(const fs::path& filePath) const
        -> std::optional<json>;

    /**
     * @brief Adds a file type to the list of tracked file types.
     * @param fileType The file type to add.
     */
    void addFileType(std::string_view fileType);

    /**
     * @brief Removes a file type from the list of tracked file types.
     * @param fileType The file type to remove.
     */
    void removeFileType(std::string_view fileType);

    /**
     * @brief Sets the encryption key for encrypting/decrypting the JSON file.
     * @param key The encryption key.
     */
    void setEncryptionKey(std::string_view key);

    /**
     * @brief Starts watching the directory for changes.
     */
    void startWatching();

    /**
     * @brief Stops watching the directory for changes.
     */
    void stopWatching();

    /**
     * @brief Sets a callback function to be called when a change is detected.
     * @param callback The callback function.
     */
    void setChangeCallback(
        std::function<void(const fs::path&, const std::string&)> callback);

    /**
     * @brief Processes a batch of files using a specified processor function.
     * @param files The files to process.
     * @param processor The processor function.
     */
    void batchProcess(const std::vector<fs::path>& files,
                      std::function<void(const fs::path&)> processor);

    /**
     * @brief Gets statistics about the tracked files.
     * @return A JSON object containing the statistics.
     */
    [[nodiscard]] auto getStatistics() const -> json;

    /**
     * @brief Enables or disables the file cache.
     * @param enable Whether to enable the cache.
     */
    void enableCache(bool enable = true);

    /**
     * @brief Sets the maximum size of the file cache.
     * @param maxSize The maximum cache size.
     */
    void setCacheSize(size_t maxSize);

    /**
     * @brief Struct for storing file statistics.
     */
    struct FileStats {
        size_t totalFiles{0};     ///< Total number of files.
        size_t modifiedFiles{0};  ///< Number of modified files.
        size_t newFiles{0};       ///< Number of new files.
        size_t deletedFiles{0};   ///< Number of deleted files.
        std::chrono::system_clock::time_point
            lastScanTime;  ///< Time of the last scan.
    } ATOM_ALIGNAS(64);

    /**
     * @brief Gets the current file statistics.
     * @return A FileStats object containing the current statistics.
     */
    [[nodiscard]] auto getCurrentStats() const -> FileStats;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}  // namespace lithium

#endif  // LITHIUM_ADDON_TRACKER_HPP
