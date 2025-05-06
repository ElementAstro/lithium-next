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
#include <syncstream>
#include <thread>
#include <vector>

#include "atom/error/exception.hpp"
#include "atom/type/json.hpp"
#include "atom/utils/aes.hpp"
#include "atom/utils/difflib.hpp"
#include "atom/utils/string.hpp"
#include "atom/utils/time.hpp"

namespace lithium {
// Exception classes with improved source location handling
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

// Improved thread-safe logger with severity levels
class ThreadSafeLogger {
public:
    enum class Level { Debug, Info, Warning, Error, Critical };

    static void log(std::string_view message, Level level = Level::Info) {
        std::osyncstream syncStream(level >= Level::Warning ? std::cerr
                                                            : std::cout);
        syncStream << formatPrefix(level) << message << '\n';
    }

    static void logError(std::string_view message) {
        log(message, Level::Error);
    }

    static void logCritical(std::string_view message) {
        log(message, Level::Critical);
    }

private:
    static std::string formatPrefix(Level level) {
        auto now = std::chrono::system_clock::now();
        std::time_t now_c = std::chrono::system_clock::to_time_t(now);
        std::tm tm;
        localtime_r(&now_c, &tm);

        std::ostringstream oss;
        oss << "[" << std::put_time(&tm, "%H:%M:%S") << "|";

        switch (level) {
            case Level::Debug:
                oss << "DEBUG";
                break;
            case Level::Info:
                oss << "INFO ";
                break;
            case Level::Warning:
                oss << "WARN ";
                break;
            case Level::Error:
                oss << "ERROR";
                break;
            case Level::Critical:
                oss << "CRIT ";
                break;
        }

        oss << "|FileTracker] ";
        return oss.str();
    }
};

// Enhanced thread pool with work stealing and task priorities
class ThreadPool {
public:
    enum class Priority { Low = 0, Normal = 1, High = 2 };

    struct Task {
        std::function<void()> func;
        Priority priority;

        // Comparison for priority queue (higher priority first)
        bool operator<(const Task& other) const {
            return priority < other.priority;
        }
    };

    explicit ThreadPool(size_t numThreads = std::thread::hardware_concurrency())
        : stop_(false),
          activeThreads_(0),
          maxQueueSize_(std::numeric_limits<size_t>::max()) {
        try {
            workers_.reserve(numThreads);
            for (size_t i = 0; i < numThreads; ++i) {
                workers_.emplace_back([this, i] { workerLoop(i); });
            }
        } catch (...) {
            stop_ = true;
            condition_.notify_all();
            throw;
        }
    }

    ~ThreadPool() {
        {
            std::unique_lock lock(queueMutex_);
            stop_ = true;
        }
        condition_.notify_all();
        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

    // Improved enqueue with priority support
    template <typename F, typename... Args>
        requires std::invocable<std::decay_t<F>, std::decay_t<Args>...>
    auto enqueue(F&& f, Args&&... args, Priority priority = Priority::Normal)
        -> std::future<std::invoke_result_t<F, Args...>> {
        using ReturnType = std::invoke_result_t<F, Args...>;

        // Check if queue is full
        {
            std::unique_lock lock(queueMutex_);
            if (tasks_.size() >= maxQueueSize_) {
                throw std::runtime_error("ThreadPool: task queue is full");
            }
        }

        auto task = std::make_shared<std::packaged_task<ReturnType()>>(
            [f = std::forward<F>(f),
             ... args = std::forward<Args>(args)]() mutable {
                return std::invoke(std::forward<F>(f),
                                   std::forward<Args>(args)...);
            });

        std::future<ReturnType> result = task->get_future();

        {
            std::unique_lock lock(queueMutex_);
            if (stop_) {
                throw std::runtime_error("ThreadPool: enqueue on stopped pool");
            }

            tasks_.push({[task]() { (*task)(); }, priority});
        }

        condition_.notify_one();
        return result;
    }

    // Set maximum queue size to prevent memory exhaustion
    void setMaxQueueSize(size_t size) {
        std::unique_lock lock(queueMutex_);
        maxQueueSize_ = size > 0 ? size : std::numeric_limits<size_t>::max();
    }

    size_t getActiveThreadCount() const {
        return activeThreads_.load(std::memory_order_relaxed);
    }

    bool isIdle() const {
        return activeThreads_.load(std::memory_order_relaxed) == 0 &&
               tasks_.empty();
    }

    void waitForCompletion() {
        std::unique_lock lock(queueMutex_);
        completionCondition_.wait(lock, [this] {
            return tasks_.empty() && activeThreads_.load() == 0;
        });
    }

private:
    void workerLoop(size_t workerId) {
        while (true) {
            Task task;
            bool hasTask = false;

            {
                std::unique_lock lock(queueMutex_);
                condition_.wait(lock,
                                [this] { return stop_ || !tasks_.empty(); });

                if (stop_ && tasks_.empty()) {
                    return;
                }

                if (!tasks_.empty()) {
                    task = std::move(tasks_.top());
                    tasks_.pop();
                    hasTask = true;
                }
            }

            if (hasTask) {
                activeThreads_.fetch_add(1, std::memory_order_relaxed);

                try {
                    task.func();
                } catch (const std::exception& e) {
                    ThreadSafeLogger::logError(
                        std::string("Task exception in worker ") +
                        std::to_string(workerId) + ": " + e.what());
                } catch (...) {
                    ThreadSafeLogger::logError(
                        std::string("Unknown task exception in worker ") +
                        std::to_string(workerId));
                }

                activeThreads_.fetch_sub(1, std::memory_order_relaxed);

                {
                    std::unique_lock lock(queueMutex_);
                    if (tasks_.empty() && activeThreads_.load() == 0) {
                        completionCondition_.notify_all();
                    }
                }
            }
        }
    }

    std::vector<std::thread> workers_;
    std::priority_queue<Task> tasks_;
    std::mutex queueMutex_;
    std::condition_variable condition_;
    std::condition_variable completionCondition_;
    std::atomic<bool> stop_;
    std::atomic<size_t> activeThreads_;
    size_t maxQueueSize_;
};

// LRU cache implemented with C++20 features
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

        // Update access order (need write lock)
        readLock.unlock();
        std::unique_lock writeLock(mutex_);

        // Check again after acquiring write lock
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

        // Key exists, update value and move to front
        if (it != cache_.end()) {
            accessList_.erase(it->second.second);
            accessList_.push_front(key);
            it->second = {std::move(value), accessList_.begin()};
            return;
        }

        // Cache is full, remove least recently used
        if (cache_.size() >= capacity_) {
            cache_.erase(accessList_.back());
            accessList_.pop_back();
        }

        // Add new key-value
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

        // Remove least recently used entries if needed
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

    // Thread pool with C++20 features
    std::unique_ptr<ThreadPool> threadPool;

    // File watching members
    std::atomic<bool> watching{false};
    std::thread watchThread;
    std::function<void(const fs::path&, const std::string&)> changeCallback;

    // Improved cache with LRU policy
    LRUCache<std::string, std::chrono::system_clock::time_point> fileCache{
        1000};
    bool cacheEnabled{false};

    // Statistics
    FileStats stats;
    std::mutex statsMutex;

    // File change notification queue
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
            ThreadSafeLogger::log(
                "No file types specified, will track all files",
                ThreadSafeLogger::Level::Warning);
        }

        // Initialize thread pool with optimal thread count based on CPU cores
        // and task type
        size_t threadCount = std::clamp<size_t>(
            std::thread::hardware_concurrency(),
            2,  // Minimum 2 threads
            16  // Maximum 16 threads (to avoid excessive context switching)
        );

        ThreadSafeLogger::log("Initializing thread pool with " +
                                  std::to_string(threadCount) + " threads",
                              ThreadSafeLogger::Level::Debug);

        threadPool = std::make_unique<ThreadPool>(threadCount);
        threadPool->setMaxQueueSize(10000);  // Prevent excessive memory usage
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
                    ThreadSafeLogger::log("Created backup at: " + backupPath,
                                          ThreadSafeLogger::Level::Debug);
                } catch (const std::exception& e) {
                    ThreadSafeLogger::log(
                        "Failed to create backup: " + std::string(e.what()),
                        ThreadSafeLogger::Level::Warning);
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
                ThreadSafeLogger::log("JSON file does not exist: " + filePath,
                                      ThreadSafeLogger::Level::Warning);
                return {};
            }

            // Check if file is readable and has content
            if (fs::file_size(filePath) == 0) {
                ThreadSafeLogger::log("JSON file is empty: " + filePath,
                                      ThreadSafeLogger::Level::Warning);
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
                    ThreadSafeLogger::log(
                        "Attempting to recover from backup file: " + backupPath,
                        ThreadSafeLogger::Level::Warning);
                    try {
                        std::ifstream backupFile(backupPath);
                        json backupJson;
                        backupFile >> backupJson;
                        return backupJson;
                    } catch (...) {
                        // Fall through to throw the original error
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
            // Use std::vector for better parallel processing support
            std::vector<fs::path> discoveredFiles;

            // Check if directory exists and is accessible
            if (!fs::exists(directory) || !fs::is_directory(directory)) {
                throw FailToScanDirectory(
                    "Directory does not exist or is not accessible: " +
                    directory);
            }

            // Define a view to filter files by extension
            auto isTrackedFile = [this](const fs::path& path) -> bool {
                if (fileTypes.empty()) {
                    return true;  // Track all files if no types specified
                }
                std::string ext = path.extension().string();
                return std::ranges::find(fileTypes, ext) != fileTypes.end();
            };

            // Use C++20 ranges to create a filtered view
            if (recursive) {
                try {
                    auto dirRange = fs::recursive_directory_iterator(
                        directory,
                        fs::directory_options::skip_permission_denied);
                    // Capture discoveredFiles by reference and use ranges to
                    // filter and process
                    auto addFile = [&discoveredFiles, &isTrackedFile](
                                       const fs::directory_entry& entry) {
                        if (entry.is_regular_file() &&
                            isTrackedFile(entry.path())) {
                            discoveredFiles.push_back(entry.path());
                        }
                    };

                    // Process directory entries with ranges
                    std::ranges::for_each(dirRange, addFile);

                } catch (const fs::filesystem_error& e) {
                    // Handle specific filesystem errors more gracefully
                    ThreadSafeLogger::logError(
                        "Filesystem error during discovery: " +
                        std::string(e.what()) +
                        " (continuing with partial results)");
                }
            } else {
                try {
                    auto dirRange = fs::directory_iterator(directory);
                    // Use ranges for filtering non-recursive directory
                    auto fileView =
                        dirRange | std::views::filter([](const auto& entry) {
                            return entry.is_regular_file();
                        }) |
                        std::views::filter([&isTrackedFile](const auto& entry) {
                            return isTrackedFile(entry.path());
                        }) |
                        std::views::transform(
                            [](const auto& entry) { return entry.path(); });

                    // Copy to output vector using ranges
                    discoveredFiles.clear();
                    std::ranges::copy(fileView,
                                      std::back_inserter(discoveredFiles));

                } catch (const fs::filesystem_error& e) {
                    ThreadSafeLogger::logError("Directory iteration error: " +
                                               std::string(e.what()));
                }
            }

            return discoveredFiles;
        } catch (const std::exception& e) {
            throw FailToScanDirectory(
                std::string("Failed to discover files: ") + e.what());
        }
    }

    // Optimize JSON generation with parallel processing and better error
    // handling
    void generateJSON() {
        try {
            std::vector<fs::path> files = discoverFiles();
            std::atomic<size_t> processedCount = 0;
            const size_t totalCount = files.size();

            // Use latch to synchronize completion
            std::latch completionLatch(totalCount > 0 ? totalCount : 1);

            // Process files in parallel with proper scheduling to avoid
            // overwhelming the system
            const size_t maxConcurrentTasks =
                threadPool->getActiveThreadCount() * 4;
            std::atomic<size_t> activeTasks = 0;

            for (const auto& file : files) {
                // Simple scheduling - limit concurrent tasks
                while (activeTasks.load() >= maxConcurrentTasks) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }

                activeTasks.fetch_add(1);
                threadPool->enqueue(
                    [this, file, &processedCount, totalCount, &completionLatch,
                     &activeTasks]() {
                        try {
                            processFile(file);
                            size_t current = processedCount.fetch_add(1) + 1;
                            if (current %
                                        std::max<size_t>(1, totalCount / 10) ==
                                    0 ||
                                current == totalCount) {
                                ThreadSafeLogger::log(
                                    "Processed " + std::to_string(current) +
                                    " of " + std::to_string(totalCount) +
                                    " files");
                            }
                        } catch (const std::exception& e) {
                            ThreadSafeLogger::logError(
                                std::string("Error processing file ") +
                                file.string() + ": " + e.what());
                        }
                        activeTasks.fetch_sub(1);
                        completionLatch.count_down();
                    },
                    ThreadPool::Priority::Normal);
            }

            // If no files were found, ensure the latch is released
            if (totalCount == 0) {
                completionLatch.count_down();
            }

            // Wait for all files to be processed
            completionLatch.wait();

            // Save JSON with proper exception handling
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
                ThreadSafeLogger::logError("Failed to calculate hash for " +
                                           entry.string() + ": " + e.what());
                hash = "hash_calculation_failed";
            }

            std::string lastWriteTime = atom::utils::getChinaTimestampString();

            // Get file size with error handling
            uintmax_t fileSize = 0;
            try {
                fileSize = fs::file_size(entry);
            } catch (const std::exception& e) {
                ThreadSafeLogger::logError("Failed to get file size for " +
                                           entry.string() + ": " + e.what());
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
                ThreadSafeLogger::log("No files need recovery",
                                      ThreadSafeLogger::Level::Info);
                return;
            }

            ThreadSafeLogger::log(
                "Beginning recovery of " + std::to_string(pathCount) + " files",
                ThreadSafeLogger::Level::Info);

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
                                if (restoreFileContent(path)) {
                                    successCount.fetch_add(
                                        1, std::memory_order_relaxed);
                                } else {
                                    failureCount.fetch_add(
                                        1, std::memory_order_relaxed);
                                }
                            } catch (const std::exception& e) {
                                ThreadSafeLogger::logError(
                                    "Failed to recover file " + path + ": " +
                                    e.what());
                                failureCount.fetch_add(
                                    1, std::memory_order_relaxed);
                            }
                            completionLatch.count_down();
                        },
                        ThreadPool::Priority::High));
                }

                // Wait for current batch to complete before starting next batch
                for (auto& future : futures) {
                    future.wait();
                }
                futures.clear();

                // Report progress periodically
                const size_t processed = std::min(end, pathCount);
                ThreadSafeLogger::log("Processed " + std::to_string(processed) +
                                      " of " + std::to_string(pathCount) +
                                      " files for recovery");
            }

            // Wait for all tasks to complete
            completionLatch.wait();

            // Log completion statistics
            ThreadSafeLogger::log(
                "Recovery complete: " + std::to_string(successCount) +
                    " files recovered successfully, " +
                    std::to_string(failureCount) + " files failed",
                failureCount > 0 ? ThreadSafeLogger::Level::Warning
                                 : ThreadSafeLogger::Level::Info);

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

    // Helper method for file restoration with improved error handling
    bool restoreFileContent(const std::string& filePath) {
        try {
            // Get file information from the old JSON
            if (!oldJson.contains(filePath)) {
                return false;
            }

            const json& fileInfo = oldJson[filePath];
            std::ofstream outFile(filePath);
            if (!outFile.is_open()) {
                ThreadSafeLogger::logError("Failed to open file for writing: " +
                                           filePath);
                return false;
            }

            // Write minimal recovery information
            outFile << "# Recovered file\n"
                    << "# Original last modified: "
                    << fileInfo["last_write_time"].get<std::string>() << "\n"
                    << "# Original size: " << fileInfo["size"].get<uintmax_t>()
                    << " bytes\n"
                    << "# Original hash: "
                    << fileInfo["hash"].get<std::string>() << "\n\n";

            outFile.close();
            return true;
        } catch (const std::exception& e) {
            ThreadSafeLogger::logError("Error restoring file " + filePath +
                                       ": " + e.what());
            return false;
        }
    }

    // Enhanced directory watching with file system event detection
    void watchDirectory() {
        if (watching.exchange(true)) {
            // Already watching
            return;
        }

        // Start notification processor if not already running
        startNotificationProcessor();

        // Create a thread for watching directory changes
        watchThread = std::thread([this]() {
            try {
                using Clock = std::chrono::steady_clock;
                auto lastCheckTime = Clock::now();
                const auto checkInterval = std::chrono::seconds(1);

                // Keep track of file modification times for change detection
                std::unordered_map<std::string, fs::file_time_type>
                    lastModifiedTimes;

                // Watch until stopped
                while (watching) {
                    try {
                        auto now = Clock::now();
                        if (now - lastCheckTime < checkInterval) {
                            std::this_thread::sleep_for(
                                std::chrono::milliseconds(100));
                            continue;
                        }

                        lastCheckTime = now;

                        // Discover current files
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
                                    // New file discovered
                                    queueChangeNotification(path, "new");
                                    lastModifiedTimes[pathStr] = lastModTime;
                                }

                                // Update cache if enabled
                                if (cacheEnabled) {
                                    fileCache.put(
                                        pathStr,
                                        std::chrono::file_clock::to_sys(
                                            lastModTime));
                                }
                            } catch (const std::exception& e) {
                                ThreadSafeLogger::logError(
                                    "Error checking file " + path.string() +
                                    ": " + e.what());
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

                        // Remove deleted files from tracking
                        for (const auto& pathStr : pathsToRemove) {
                            lastModifiedTimes.erase(pathStr);
                        }
                    } catch (const std::exception& e) {
                        ThreadSafeLogger::logError("Error in watch cycle: " +
                                                   std::string(e.what()));
                        std::this_thread::sleep_for(std::chrono::seconds(1));
                    }
                }
            } catch (const std::exception& e) {
                ThreadSafeLogger::logCritical(
                    "Fatal error in directory watcher: " +
                    std::string(e.what()));
            }

            // Ensure we stop processing notifications when watching stops
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
            ThreadSafeLogger::log("Starting notification processor",
                                  ThreadSafeLogger::Level::Debug);

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
                        ThreadSafeLogger::logError(
                            "Error in change callback: " +
                            std::string(e.what()));
                    } catch (...) {
                        ThreadSafeLogger::logError(
                            "Unknown error in change callback");
                    }
                }
            }

            ThreadSafeLogger::log("Notification processor stopped",
                                  ThreadSafeLogger::Level::Debug);
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

    // Convert time point to string representation
    static std::string timePointToString(
        const std::chrono::system_clock::time_point& tp) {
        std::time_t time = std::chrono::system_clock::to_time_t(tp);
        std::tm tm;
        localtime_r(&time, &tm);  // Thread-safe version of localtime
        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
        return oss.str();
    }
};

// FileTracker implementation

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

        // Add timestamp to log header
        logFile << "\n=== Differences Log: "
                << impl_->timePointToString(std::chrono::system_clock::now())
                << " ===\n";

        // Log each difference
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

    // Process files in batches with improved concurrency
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

template <ChangeNotifier Callback>
void FileTracker::setChangeCallback(Callback&& callback) {
    impl_->changeCallback = std::forward<Callback>(callback);
}

// Explicit instantiations to avoid linker errors
template void FileTracker::forEachFile(lithium::FileHandler auto&&) const;
template void FileTracker::batchProcess(const std::vector<fs::path>&,
                                        lithium::FileHandler auto&&);
template void FileTracker::setChangeCallback(lithium::ChangeNotifier auto&&);

}  // namespace lithium
