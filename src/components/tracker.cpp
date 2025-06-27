#include "tracker.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <functional>
#include <future>
#include <iomanip>
#include <iostream>
#include <latch>
#include <mutex>
#include <optional>
#include <queue>
#include <ranges>
#include <set>
#include <shared_mutex>
#include <source_location>
#include <span>
#include <thread>
#include <vector>

#include "atom/async/pool.hpp"
#include "atom/error/exception.hpp"
#include "atom/type/json.hpp"
#include "atom/utils/aes.hpp"
#include "atom/utils/difflib.hpp"
#include "atom/utils/string.hpp"
#include "atom/utils/time.hpp"
#include "spdlog/spdlog.h"

namespace lithium {

class FailToScanDirectory : public atom::error::Exception {
public:
    using Exception::Exception;
    explicit FailToScanDirectory(
        const std::string& message,
        const std::source_location& loc = std::source_location::current())
        : Exception(loc.file_name(), loc.line(), loc.function_name(), message) {
    }
};

class FailToCompareJSON : public atom::error::Exception {
public:
    using Exception::Exception;
    explicit FailToCompareJSON(
        const std::string& message,
        const std::source_location& loc = std::source_location::current())
        : Exception(loc.file_name(), loc.line(), loc.function_name(), message) {
    }
};

class FailToLogDifferences : public atom::error::Exception {
public:
    using Exception::Exception;
    explicit FailToLogDifferences(
        const std::string& message,
        const std::source_location& loc = std::source_location::current())
        : Exception(loc.file_name(), loc.line(), loc.function_name(), message) {
    }
};

class FailToRecoverFiles : public atom::error::Exception {
public:
    using Exception::Exception;
    explicit FailToRecoverFiles(
        const std::string& message,
        const std::source_location& loc = std::source_location::current())
        : Exception(loc.file_name(), loc.line(), loc.function_name(), message) {
    }
};

class FailToOpenFile : public atom::error::Exception {
public:
    using Exception::Exception;
    explicit FailToOpenFile(
        const std::string& message,
        const std::source_location& loc = std::source_location::current())
        : Exception(loc.file_name(), loc.line(), loc.function_name(), message) {
    }
};

template <typename K, typename V>
class LRUCache {
public:
    explicit LRUCache(size_t capacity) : capacity_(capacity) {}

    std::optional<V> get(const K& key) {
        std::shared_lock readLock(mutex_);
        auto it = cache_.find(key);
        if (it == cache_.end()) {
            return std::nullopt;
        }

        readLock.unlock();
        std::unique_lock writeLock(mutex_);

        it = cache_.find(key);
        if (it == cache_.end()) {
            return std::nullopt;
        }

        accessList_.erase(it->second.second);
        accessList_.push_front(key);
        it->second.second = accessList_.begin();
        return it->second.first;
    }

    void put(K key, V value) {
        std::unique_lock lock(mutex_);
        auto it = cache_.find(key);

        if (it != cache_.end()) {
            accessList_.erase(it->second.second);
            accessList_.push_front(key);
            it->second = {std::move(value), accessList_.begin()};
            return;
        }

        if (cache_.size() >= capacity_) {
            cache_.erase(accessList_.back());
            accessList_.pop_back();
        }

        accessList_.push_front(key);
        cache_[key] = {std::move(value), accessList_.begin()};
    }

    void clear() {
        std::unique_lock lock(mutex_);
        cache_.clear();
        accessList_.clear();
    }

    bool contains(const K& key) const {
        std::shared_lock lock(mutex_);
        return cache_.contains(key);
    }

    size_t size() const {
        std::shared_lock lock(mutex_);
        return cache_.size();
    }

    size_t capacity() const { return capacity_; }

    void setCapacity(size_t newCapacity) {
        if (newCapacity == 0) {
            throw std::invalid_argument("Cache capacity cannot be zero");
        }

        std::unique_lock lock(mutex_);
        capacity_ = newCapacity;

        while (cache_.size() > capacity_) {
            cache_.erase(accessList_.back());
            accessList_.pop_back();
        }
    }

private:
    size_t capacity_;
    std::list<K> accessList_;
    std::unordered_map<K, std::pair<V, typename std::list<K>::iterator>> cache_;
    mutable std::shared_mutex mutex_;
};

struct FileTracker::Impl {
    std::string directory;
    std::string jsonFilePath;
    bool recursive;
    std::vector<std::string> fileTypes;
    json newJson;
    json oldJson;
    json differences;
    std::shared_mutex mtx;
    std::optional<std::string> encryptionKey;

    std::unique_ptr<atom::async::ThreadPool<>> threadPool;

    std::atomic<bool> watching{false};
    std::thread watchThread;
    std::function<void(const fs::path&, const std::string&)> changeCallback;

    LRUCache<std::string, std::chrono::system_clock::time_point> fileCache{
        1000};
    bool cacheEnabled{false};

    FileStats stats;
    std::mutex statsMutex;

    struct ChangeNotification {
        fs::path path;
        std::string changeType;
        std::chrono::system_clock::time_point timestamp =
            std::chrono::system_clock::now();
    };
    std::queue<ChangeNotification> changeQueue;
    std::mutex changeQueueMutex;
    std::condition_variable changeQueueCV;
    std::thread notificationThread;
    std::atomic<bool> processingNotifications{false};

    Impl(std::string_view dir, std::string_view jFilePath,
         std::span<const std::string> types, bool rec)
        : directory(dir),
          jsonFilePath(jFilePath),
          recursive(rec),
          fileTypes(types.begin(), types.end()) {
        // Validate directory
        if (!fs::exists(directory)) {
            throw std::invalid_argument("Directory does not exist: " +
                                        std::string(dir));
        }

        if (jsonFilePath.empty()) {
            throw std::invalid_argument("JSON file path cannot be empty");
        }

        if (fileTypes.empty()) {
            spdlog::warn("No file types specified, will track all files");
        }

        // Initialize thread pool with optimal thread count based on CPU cores
        // and task type
        size_t threadCount =
            std::clamp<size_t>(std::thread::hardware_concurrency(), 2, 16);
        spdlog::debug("Initializing thread pool with {} threads", threadCount);

        threadPool = std::make_unique<atom::async::ThreadPool<>>(threadCount);
    }

    ~Impl() {
        bool wasWatching = watching.exchange(false);
        if (wasWatching && watchThread.joinable()) {
            watchThread.join();
        }
        if (notificationThread.joinable()) {
            processingNotifications = false;
            changeQueueCV.notify_all();
            notificationThread.join();
        }
    }

    // Enhanced JSON saving with validation and backup
    static void saveJSON(const json& j, const std::string& filePath,
                         const std::optional<std::string>& key) {
        try {
            // Create backup of existing file if it exists
            if (fs::exists(filePath)) {
                auto backupPath = filePath + ".backup";
                try {
                    fs::copy_file(filePath, backupPath,
                                  fs::copy_options::overwrite_existing);
                    spdlog::debug("Created backup at: {}", backupPath);
                } catch (const std::exception& e) {
                    spdlog::warn("Failed to create backup: {}", e.what());
                }
            }

            // Validate JSON first
            if (j.is_discarded() || j.is_null()) {
                throw std::invalid_argument("Invalid JSON data");
            }

            // Ensure directory exists
            fs::path path(filePath);
            fs::create_directories(path.parent_path());

            // Write to temporary file first
            std::string tempPath = filePath + ".tmp";
            {
                std::ofstream outFile(tempPath, std::ios::binary);
                if (!outFile.is_open()) {
                    throw FailToOpenFile(
                        "Failed to open temporary file for writing: " +
                        tempPath);
                }

                if (key && !key->empty()) {
                    std::vector<unsigned char> iv;
                    std::vector<unsigned char> tag;
                    std::string encrypted =
                        atom::utils::encryptAES(j.dump(), *key, iv, tag);
                    outFile.write(encrypted.data(), encrypted.size());
                } else {
                    outFile << std::setw(4) << j << std::endl;
                }
                outFile.close();

                // Check file was written correctly
                if (!fs::exists(tempPath) || fs::file_size(tempPath) == 0) {
                    throw std::runtime_error(
                        "Failed to write data to temporary file");
                }
            }

            // Rename temporary file to actual file (atomic operation)
            fs::rename(tempPath, filePath);

        } catch (const FailToOpenFile&) {
            throw;
        } catch (const std::exception& e) {
            throw FailToOpenFile(std::string("Error saving JSON: ") + e.what());
        }
    }

    // Improved JSON loading with validation and error handling
    static auto loadJSON(const std::string& filePath,
                         const std::optional<std::string>& key) -> json {
        try {
            if (!fs::exists(filePath)) {
                spdlog::warn("JSON file does not exist: {}", filePath);
                return {};
            }

            // Check if file is readable and has content
            if (fs::file_size(filePath) == 0) {
                spdlog::warn("JSON file is empty: {}", filePath);
                return {};
            }

            std::ifstream inFile(filePath, std::ios::binary);
            if (!inFile.is_open()) {
                throw FailToOpenFile("Failed to open file for reading: " +
                                     filePath);
            }

            if (key && !key->empty()) {
                std::string encrypted((std::istreambuf_iterator<char>(inFile)),
                                      std::istreambuf_iterator<char>());
                std::vector<unsigned char> iv;
                std::vector<unsigned char> tag;
                try {
                    std::string decrypted =
                        atom::utils::decryptAES(encrypted, *key, iv, tag);
                    return json::parse(decrypted);
                } catch (const json::parse_error& e) {
                    throw FailToOpenFile("JSON parse error after decryption: " +
                                         std::string(e.what()));
                } catch (const std::exception& e) {
                    throw FailToOpenFile(
                        std::string("Error decrypting JSON: ") + e.what());
                }
            }

            try {
                json j;
                inFile >> j;
                return j;
            } catch (const json::parse_error& e) {
                // Try to recover from backup if parse error
                auto backupPath = filePath + ".backup";
                if (fs::exists(backupPath)) {
                    spdlog::warn("Attempting to recover from backup file: {}",
                                 backupPath);
                    try {
                        std::ifstream backupFile(backupPath);
                        json backupJson;
                        backupFile >> backupJson;
                        return backupJson;
                    } catch (...) {
                    }
                }
                throw FailToOpenFile("JSON parse error: " +
                                     std::string(e.what()));
            }
        } catch (const FailToOpenFile&) {
            throw;
        } catch (const std::exception& e) {
            throw FailToOpenFile(std::string("Error loading JSON: ") +
                                 e.what());
        }
    }

    // Optimized file discovery using C++20 ranges and parallel processing
    auto discoverFiles() -> std::vector<fs::path> {
        try {
            std::vector<fs::path> discoveredFiles;

            if (!fs::exists(directory) || !fs::is_directory(directory)) {
                throw FailToScanDirectory(
                    "Directory does not exist or is not accessible: " +
                    directory);
            }

            auto isTrackedFile = [this](const fs::path& path) -> bool {
                if (fileTypes.empty()) {
                    return true;  // Track all files if no types specified
                }
                std::string ext = path.extension().string();
                return std::ranges::find(fileTypes, ext) != fileTypes.end();
            };

            if (recursive) {
                try {
                    auto dirRange = fs::recursive_directory_iterator(
                        directory,
                        fs::directory_options::skip_permission_denied);

                    auto addFile = [&discoveredFiles, &isTrackedFile](
                                       const fs::directory_entry& entry) {
                        if (entry.is_regular_file() &&
                            isTrackedFile(entry.path())) {
                            discoveredFiles.push_back(entry.path());
                        }
                    };

                    std::ranges::for_each(dirRange, addFile);
                } catch (const fs::filesystem_error& e) {
                    spdlog::error(
                        "Filesystem error during discovery: {} (continuing "
                        "with partial results)",
                        e.what());
                }
            } else {
                // Non-recursive directory scanning
                try {
                    auto dirRange = fs::directory_iterator(directory);
                    auto fileView =
                        dirRange | std::views::filter([](const auto& entry) {
                            return entry.is_regular_file();
                        }) |
                        std::views::filter([&isTrackedFile](const auto& entry) {
                            return isTrackedFile(entry.path());
                        }) |
                        std::views::transform(
                            [](const auto& entry) { return entry.path(); });

                    std::ranges::copy(fileView,
                                      std::back_inserter(discoveredFiles));
                } catch (const fs::filesystem_error& e) {
                    spdlog::error("Directory iteration error: {}", e.what());
                }
            }

            return discoveredFiles;
        } catch (const std::exception& e) {
            throw FailToScanDirectory(
                std::string("Failed to discover files: ") + e.what());
        }
    }

    void generateJSON() {
        try {
            std::vector<fs::path> files = discoverFiles();
            std::atomic<size_t> processedCount = 0;
            const size_t totalCount = files.size();

            std::latch completionLatch(totalCount > 0 ? totalCount : 1);

            // Use thread pool thread count for concurrency control
            const size_t maxConcurrentTasks = threadPool->size() * 4;
            std::atomic<size_t> activeTasks = 0;

            for (const auto& file : files) {
                while (activeTasks.load() >= maxConcurrentTasks) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }

                activeTasks.fetch_add(1);
                // Cast to void to handle [[nodiscard]] attribute
                (void)threadPool->enqueue([this, file, &processedCount,
                                           totalCount, &completionLatch,
                                           &activeTasks]() {
                    try {
                        processFile(file);
                        size_t current = processedCount.fetch_add(1) + 1;
                        if (current % std::max<size_t>(1, totalCount / 10) ==
                                0 ||
                            current == totalCount) {
                            spdlog::info("Processed {} of {} files", current,
                                         totalCount);
                        }
                    } catch (const std::exception& e) {
                        spdlog::error("Error processing file {}: {}",
                                      file.string(), e.what());
                    }
                    activeTasks.fetch_sub(1);
                    completionLatch.count_down();
                });
            }

            if (totalCount == 0) {
                completionLatch.count_down();
            }

            completionLatch.wait();
            saveJSON(newJson, jsonFilePath, encryptionKey);
        } catch (const std::exception& e) {
            throw FailToScanDirectory(std::string("Failed to generate JSON: ") +
                                      e.what());
        }
    }

    // Process file with enhanced error handling and optimizations
    void processFile(const fs::path& entry) {
        try {
            if (!fs::exists(entry) || !fs::is_regular_file(entry)) {
                return;
            }

            // Calculate hash with proper error handling
            std::string hash;
            try {
                hash = atom::utils::calculateSha256(entry.string());
            } catch (const std::exception& e) {
                spdlog::error("Failed to calculate hash for {}: {}",
                              entry.string(), e.what());
                hash = "hash_calculation_failed";
            }

            std::string lastWriteTime = atom::utils::getChinaTimestampString();

            // Get file size with error handling
            uintmax_t fileSize = 0;
            try {
                fileSize = fs::file_size(entry);
            } catch (const std::exception& e) {
                spdlog::error("Failed to get file size for {}: {}",
                              entry.string(), e.what());
                fileSize = 0;
            }

            // Use shared mutex for better concurrency
            std::unique_lock lock(mtx);
            newJson[entry.string()] = {{"last_write_time", lastWriteTime},
                                       {"hash", hash},
                                       {"size", fileSize},
                                       {"type", entry.extension().string()}};
        } catch (const std::exception& e) {
            throw std::runtime_error(std::string("Error processing file: ") +
                                     e.what());
        }
    }

    // Improved JSON comparison with optimizations
    auto compareJSON() -> json {
        try {
            json diff;

            // Create sets for faster lookup
            std::set<std::string> oldPaths;
            std::set<std::string> newPaths;

            for (const auto& [path, _] : oldJson.items()) {
                oldPaths.insert(path);
            }

            for (const auto& [path, _] : newJson.items()) {
                newPaths.insert(path);
            }

            // Find modified files (present in both old and new)
            std::vector<std::string> modifiedPaths;
            std::set_intersection(oldPaths.begin(), oldPaths.end(),
                                  newPaths.begin(), newPaths.end(),
                                  std::back_inserter(modifiedPaths));

            // Process modified files
            for (const auto& filePath : modifiedPaths) {
                if (oldJson[filePath]["hash"] != newJson[filePath]["hash"]) {
                    // Generate detailed diff
                    std::vector<std::string> oldLines =
                        atom::utils::splitString(oldJson[filePath].dump(),
                                                 '\n');
                    std::vector<std::string> newLines =
                        atom::utils::splitString(newJson[filePath].dump(),
                                                 '\n');
                    auto differences = atom::utils::Differ::unifiedDiff(
                        oldLines, newLines, "old", "new");
                    diff[filePath] = {{"status", "modified"},
                                      {"diff", differences}};
                }
            }

            // Find new files (in new but not in old)
            std::vector<std::string> newFilePaths;
            std::set_difference(newPaths.begin(), newPaths.end(),
                                oldPaths.begin(), oldPaths.end(),
                                std::back_inserter(newFilePaths));

            // Process new files
            for (const auto& filePath : newFilePaths) {
                diff[filePath] = {{"status", "new"}};
            }

            // Find deleted files (in old but not in new)
            std::vector<std::string> deletedPaths;
            std::set_difference(oldPaths.begin(), oldPaths.end(),
                                newPaths.begin(), newPaths.end(),
                                std::back_inserter(deletedPaths));

            // Process deleted files
            for (const auto& filePath : deletedPaths) {
                diff[filePath] = {{"status", "deleted"}};
            }

            return diff;
        } catch (const std::exception& e) {
            throw FailToCompareJSON(std::string("Failed to compare JSON: ") +
                                    e.what());
        }
    }

    // Improved file recovery with parallel processing
    void recoverFiles() {
        try {
            // Prepare a list of paths that need recovery
            std::vector<std::string> pathsToRecover;
            for (const auto& [path, _] : oldJson.items()) {
                if (!fs::exists(path)) {
                    pathsToRecover.push_back(path);
                }
            }

            // Use parallel algorithms for faster processing when possible
            const size_t pathCount = pathsToRecover.size();

            if (pathCount == 0) {
                spdlog::info("No files need recovery");
                return;
            }

            spdlog::info("Beginning recovery of {} files", pathCount);

            // Use latch for synchronization with C++20 features
            std::latch completionLatch(pathCount);
            std::atomic<size_t> successCount = 0;
            std::atomic<size_t> failureCount = 0;

            // Process files in parallel with bounded concurrency
            const size_t batchSize = std::min<size_t>(100, pathCount);
            std::vector<std::future<void>> futures;
            futures.reserve(batchSize);

            for (size_t i = 0; i < pathCount; i += batchSize) {
                const size_t end = std::min(i + batchSize, pathCount);

                for (size_t j = i; j < end; ++j) {
                    futures.push_back(threadPool->enqueue(
                        [this, path = pathsToRecover[j], &completionLatch,
                         &successCount, &failureCount]() {
                            try {
                                // Create directory structure if needed
                                fs::path filePath(path);
                                fs::create_directories(filePath.parent_path());

                                // Create or restore the file
                                if (restoreFileContent(path, this->oldJson)) {
                                    successCount.fetch_add(
                                        1, std::memory_order_relaxed);
                                } else {
                                    failureCount.fetch_add(
                                        1, std::memory_order_relaxed);
                                }
                            } catch (const std::exception& e) {
                                spdlog::error("Failed to recover file {}: {}",
                                              path, e.what());
                                failureCount.fetch_add(
                                    1, std::memory_order_relaxed);
                            }
                            completionLatch.count_down();
                        }));
                }

                // Wait for current batch to complete before starting next batch
                for (auto& future : futures) {
                    future.wait();
                }
                futures.clear();

                // Report progress periodically
                const size_t processed = std::min(end, pathCount);
                spdlog::info("Processed {} of {} files for recovery", processed,
                             pathCount);
            }

            // Wait for all tasks to complete
            completionLatch.wait();

            // Log completion statistics
            spdlog::info(
                "Recovery complete: {} files recovered successfully, {} files "
                "failed",
                successCount.load(), failureCount.load());

            if (failureCount > 0) {
                throw FailToRecoverFiles(
                    "Failed to recover " + std::to_string(failureCount) +
                    " out of " + std::to_string(pathCount) + " files");
            }
        } catch (const std::exception& e) {
            throw FailToRecoverFiles(std::string("Failed to recover files: ") +
                                     e.what());
        }
    }

    // Enhanced directory watching with file system event detection
    void watchDirectory() {
        if (watching.exchange(true)) {
            return;
        }

        startNotificationProcessor();

        watchThread = std::thread([this]() {
            try {
                using Clock = std::chrono::steady_clock;
                auto lastCheckTime = Clock::now();
                const auto checkInterval = std::chrono::seconds(1);

                std::unordered_map<std::string, fs::file_time_type>
                    lastModifiedTimes;

                while (watching) {
                    try {
                        auto now = Clock::now();
                        if (now - lastCheckTime < checkInterval) {
                            std::this_thread::sleep_for(
                                std::chrono::milliseconds(100));
                            continue;
                        }

                        lastCheckTime = now;
                        auto files = discoverFiles();

                        // Check for modified or new files
                        for (const auto& path : files) {
                            try {
                                auto lastModTime = fs::last_write_time(path);
                                std::string pathStr = path.string();

                                if (lastModifiedTimes.contains(pathStr)) {
                                    if (lastModTime !=
                                        lastModifiedTimes[pathStr]) {
                                        queueChangeNotification(path,
                                                                "modified");
                                        lastModifiedTimes[pathStr] =
                                            lastModTime;
                                    }
                                } else {
                                    queueChangeNotification(path, "new");
                                    lastModifiedTimes[pathStr] = lastModTime;
                                }

                                if (cacheEnabled) {
                                    fileCache.put(
                                        pathStr,
                                        std::chrono::file_clock::to_sys(
                                            lastModTime));
                                }
                            } catch (const std::exception& e) {
                                spdlog::error("Error checking file {}: {}",
                                              path.string(), e.what());
                            }
                        }

                        // Check for deleted files
                        std::vector<std::string> pathsToRemove;
                        for (const auto& [pathStr, _] : lastModifiedTimes) {
                            bool stillExists = false;
                            for (const auto& path : files) {
                                if (path.string() == pathStr) {
                                    stillExists = true;
                                    break;
                                }
                            }

                            if (!stillExists) {
                                pathsToRemove.push_back(pathStr);
                                queueChangeNotification(fs::path(pathStr),
                                                        "deleted");
                            }
                        }

                        for (const auto& pathStr : pathsToRemove) {
                            lastModifiedTimes.erase(pathStr);
                        }
                    } catch (const std::exception& e) {
                        spdlog::error("Error in watch cycle: {}", e.what());
                        std::this_thread::sleep_for(std::chrono::seconds(1));
                    }
                }
            } catch (const std::exception& e) {
                spdlog::critical("Fatal error in directory watcher: {}",
                                 e.what());
            }

            stopNotificationProcessor();
        });
    }

    // Start the notification processor thread
    void startNotificationProcessor() {
        if (processingNotifications.exchange(true)) {
            // Already processing
            return;
        }

        notificationThread = std::thread([this]() {
            spdlog::debug("Starting notification processor");

            while (processingNotifications || !changeQueue.empty()) {
                ChangeNotification notification;
                bool hasNotification = false;

                {
                    std::unique_lock<std::mutex> lock(changeQueueMutex);
                    changeQueueCV.wait(lock, [this] {
                        return !processingNotifications || !changeQueue.empty();
                    });

                    if (!changeQueue.empty()) {
                        notification = std::move(changeQueue.front());
                        changeQueue.pop();
                        hasNotification = true;
                    }
                }

                if (hasNotification && changeCallback) {
                    try {
                        // Process notification with callback
                        changeCallback(notification.path,
                                       notification.changeType);
                    } catch (const std::exception& e) {
                        spdlog::error("Error in change callback: {}", e.what());
                    } catch (...) {
                        spdlog::error("Unknown error in change callback");
                    }
                }
            }

            spdlog::debug("Notification processor stopped");
        });
    }

    // Stop the notification processor thread
    void stopNotificationProcessor() {
        if (!processingNotifications.exchange(false)) {
            // Not processing
            return;
        }

        changeQueueCV.notify_all();

        if (notificationThread.joinable()) {
            notificationThread.join();
        }
    }

    // Queue a change notification
    void queueChangeNotification(const fs::path& path,
                                 const std::string& changeType) {
        if (!changeCallback) {
            return;
        }

        {
            std::lock_guard<std::mutex> lock(changeQueueMutex);
            changeQueue.push({path, changeType});
        }

        changeQueueCV.notify_one();
    }

    // Update stats with thread safety
    void updateStats() {
        std::lock_guard<std::mutex> lock(statsMutex);
        stats.lastScanTime = std::chrono::system_clock::now();
        stats.totalFiles = newJson.size();

        // Reset counters
        stats.modifiedFiles = 0;
        stats.newFiles = 0;
        stats.deletedFiles = 0;

        // Count differences by type
        for (const auto& [path, info] : differences.items()) {
            const std::string& status = info["status"];
            if (status == "modified") {
                stats.modifiedFiles++;
            } else if (status == "new") {
                stats.newFiles++;
            } else if (status == "deleted") {
                stats.deletedFiles++;
            }
        }
    }

    static std::string timePointToString(
        const std::chrono::system_clock::time_point& tp) {
        std::time_t time = std::chrono::system_clock::to_time_t(tp);
        std::tm tm;
#ifdef _WIN32
        localtime_s(&tm, &time);
#else
        localtime_r(&time, &tm);
#endif
        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
        return oss.str();
    }

    static bool restoreFileContent(const std::string& path,
                                   const json& oldJson) {
        try {
            // Check if the file already exists; if so, no need to restore an
            // empty one.
            if (fs::exists(path)) {
                spdlog::debug("File {} already exists, skipping restore.",
                              path);
                return true;
            }

            // Ensure parent directories exist
            fs::path filePath(path);
            fs::create_directories(filePath.parent_path());

            // Attempt to restore content if it was stored (unlikely with
            // current processFile)
            auto it = oldJson.find(path);
            if (it != oldJson.end() && (*it).contains("content") &&
                (*it)["content"].is_string()) {
                std::string content = (*it)["content"];
                std::ofstream ofs(path, std::ios::binary);
                if (!ofs.is_open()) {
                    spdlog::error("Failed to open file for restore: {}", path);
                    return false;
                }
                ofs << content;
                ofs.close();
                spdlog::info("File {} restored with content from JSON.", path);
                return true;
            } else {
                // If no content is stored, create an empty file as a
                // placeholder
                std::ofstream ofs(path);  // Creates an empty file
                if (!ofs.is_open()) {
                    spdlog::error("Failed to create empty file for restore: {}",
                                  path);
                    return false;
                }
                ofs.close();
                spdlog::warn(
                    "File {} restored as empty. Content was not tracked or "
                    "found in JSON.",
                    path);
                return true;
            }
        } catch (const std::exception& e) {
            spdlog::error("Exception in restoreFileContent for {}: {}", path,
                          e.what());
            return false;
        }
    }
};

FileTracker::FileTracker(std::string_view directory,
                         std::string_view jsonFilePath,
                         std::span<const std::string> fileTypes, bool recursive)
    : impl_(std::make_unique<Impl>(directory, jsonFilePath, fileTypes,
                                   recursive)) {}

FileTracker::~FileTracker() = default;

FileTracker::FileTracker(FileTracker&&) noexcept = default;
auto FileTracker::operator=(FileTracker&&) noexcept -> FileTracker& = default;

void FileTracker::scan() {
    try {
        if (std::filesystem::exists(impl_->jsonFilePath)) {
            impl_->oldJson =
                impl_->loadJSON(impl_->jsonFilePath, impl_->encryptionKey);
        }
        impl_->generateJSON();
        impl_->updateStats();
    } catch (const std::exception& e) {
        throw FailToScanDirectory("Scan failed: " + std::string(e.what()));
    }
}

void FileTracker::compare() {
    try {
        impl_->differences = impl_->compareJSON();
        impl_->updateStats();
    } catch (const std::exception& e) {
        throw FailToCompareJSON("Compare failed: " + std::string(e.what()));
    }
}

void FileTracker::logDifferences(std::string_view logFilePath) const {
    try {
        std::ofstream logFile(logFilePath.data(), std::ios::app);
        if (!logFile.is_open()) {
            throw FailToOpenFile("Failed to open log file: " +
                                 std::string(logFilePath));
        }

        logFile << "\n=== Differences Log: "
                << impl_->timePointToString(std::chrono::system_clock::now())
                << " ===\n";

        size_t count = 0;
        for (const auto& [filePath, info] : impl_->differences.items()) {
            logFile << "File: " << filePath << ", Status: " << info["status"]
                    << std::endl;

            if (info.contains("diff")) {
                for (const auto& line : info["diff"]) {
                    logFile << "  " << line << std::endl;
                }
            }

            count++;
        }

        logFile << "=== Total changes: " << count << " ===\n";

    } catch (const std::exception& e) {
        throw FailToLogDifferences("Logging failed: " + std::string(e.what()));
    }
}

void FileTracker::recover(std::string_view jsonFilePath) {
    try {
        if (jsonFilePath.empty()) {
            throw std::invalid_argument("JSON file path cannot be empty");
        }

        impl_->oldJson =
            impl_->loadJSON(jsonFilePath.data(), impl_->encryptionKey);
        impl_->recoverFiles();
    } catch (const std::invalid_argument&) {
        throw;
    } catch (const std::exception& e) {
        throw FailToRecoverFiles("Recovery failed: " + std::string(e.what()));
    }
}

void FileTracker::setEncryptionKey(std::string_view key) {
    if (key.empty()) {
        throw std::invalid_argument("Encryption key cannot be empty");
    }

    impl_->encryptionKey = std::string(key);
}

auto FileTracker::asyncScan() -> std::future<void> {
    return std::async(std::launch::async, [this] { scan(); });
}

auto FileTracker::asyncCompare() -> std::future<void> {
    return std::async(std::launch::async, [this] { compare(); });
}

[[nodiscard]] auto FileTracker::getDifferences() const noexcept -> const json& {
    return impl_->differences;
}

[[nodiscard]] auto FileTracker::getTrackedFileTypes() const noexcept
    -> const std::vector<std::string>& {
    return impl_->fileTypes;
}

[[nodiscard]] auto FileTracker::getFileInfo(const fs::path& filePath) const
    -> std::optional<json> {
    std::shared_lock lock(impl_->mtx);

    auto it = impl_->newJson.find(filePath.string());
    if (it == impl_->newJson.end()) {
        return std::nullopt;
    }

    return *it;
}

void FileTracker::addFileType(std::string_view fileType) {
    if (fileType.empty()) {
        throw std::invalid_argument("File type cannot be empty");
    }

    std::unique_lock lock(impl_->mtx);
    impl_->fileTypes.push_back(std::string(fileType));
}

void FileTracker::removeFileType(std::string_view fileType) {
    std::unique_lock lock(impl_->mtx);

    auto it =
        std::find(impl_->fileTypes.begin(), impl_->fileTypes.end(), fileType);
    if (it != impl_->fileTypes.end()) {
        impl_->fileTypes.erase(it);
    }
}

void FileTracker::enableCache(bool enable) {
    impl_->cacheEnabled = enable;

    if (!enable) {
        impl_->fileCache.clear();
    }
}

void FileTracker::setCacheSize(size_t maxSize) {
    if (maxSize == 0) {
        throw std::invalid_argument("Cache size cannot be zero");
    }

    impl_->fileCache.setCapacity(maxSize);
}

auto FileTracker::getStatistics() const -> json {
    std::lock_guard<std::mutex> lock(impl_->statsMutex);

    return {{"total_files", impl_->stats.totalFiles},
            {"modified_files", impl_->stats.modifiedFiles},
            {"new_files", impl_->stats.newFiles},
            {"deleted_files", impl_->stats.deletedFiles},
            {"last_scan_time",
             impl_->timePointToString(impl_->stats.lastScanTime)}};
}

auto FileTracker::getCurrentStats() const -> FileStats {
    std::lock_guard<std::mutex> lock(impl_->statsMutex);
    return impl_->stats;
}

void FileTracker::startWatching() { impl_->watchDirectory(); }

void FileTracker::stopWatching() {
    bool wasWatching = impl_->watching.exchange(false);

    if (wasWatching && impl_->watchThread.joinable()) {
        impl_->watchThread.join();
    }
}

// Template method specialization for different callback types
template <FileHandler Func>
void FileTracker::forEachFile(Func&& func) const {
    std::shared_lock lock(impl_->mtx);

    for (const auto& [path, _] : impl_->newJson.items()) {
        fs::path filePath(path);
        func(filePath);
    }
}

template <FileHandler Processor>
void FileTracker::batchProcess(const std::vector<fs::path>& files,
                               Processor&& processor) {
    if (files.empty()) {
        throw std::invalid_argument("Files list cannot be empty");
    }

    const size_t batchSize = std::min<size_t>(100, files.size());

    for (size_t i = 0; i < files.size(); i += batchSize) {
        const size_t end = std::min(i + batchSize, files.size());
        std::vector<std::future<void>> futures;
        futures.reserve(end - i);

        for (size_t j = i; j < end; ++j) {
            futures.push_back(impl_->threadPool->enqueue(
                [&processor, file = files[j]]() { processor(file); }));
        }

        // Wait for current batch to complete
        for (auto& future : futures) {
            future.wait();
        }
    }
}

void FileTracker::setChangeCallback(
    std::function<void(const fs::path&, const std::string&)> callback) {
    impl_->changeCallback = std::move(callback);
}

template <ChangeNotifier Callback>
void FileTracker::setChangeCallback(Callback&& callback) {
    impl_->changeCallback = std::forward<Callback>(callback);
}

template void FileTracker::forEachFile(lithium::FileHandler auto&&) const;
template void FileTracker::batchProcess(const std::vector<fs::path>&,
                                        lithium::FileHandler auto&&);
template void FileTracker::setChangeCallback(lithium::ChangeNotifier auto&&);

}  // namespace lithium
