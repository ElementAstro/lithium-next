#include "tracker.hpp"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <functional>
#include <future>
#include <iomanip>
#include <mutex>
#include <optional>
#include <queue>
#include <shared_mutex>
#include <thread>
#include <vector>

#include "atom/error/exception.hpp"
#include "atom/type/json.hpp"
#include "atom/utils/aes.hpp"
#include "atom/utils/difflib.hpp"
#include "atom/utils/string.hpp"
#include "atom/utils/time.hpp"

namespace lithium {
class FailToScanDirectory : public atom::error::Exception {
public:
    using Exception::Exception;
};

#define THROW_FAIL_TO_SCAN_DIRECTORY(...)                              \
    throw lithium::FailToScanDirectory(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                       ATOM_FUNC_NAME, __VA_ARGS__)

#define THROW_NESTED_FAIL_TO_SCAN_DIRECTORY(...) \
    lithium::FailToScanDirectory::rethrowNested( \
        ATOM_FILE_NAME, ATOM_FILE_LINE, ATOM_FUNC_NAME, __VA_ARGS__)

class FailToCompareJSON : public atom::error::Exception {
public:
    using Exception::Exception;
};

#define THROW_FAIL_TO_COMPARE_JSON(...)                              \
    throw lithium::FailToCompareJSON(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                     ATOM_FUNC_NAME, __VA_ARGS__)

#define THROW_NESTED_FAIL_TO_COMPARE_JSON(...)                                \
    lithium::FailToCompareJSON::rethrowNested(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                              ATOM_FUNC_NAME, __VA_ARGS__)

class FailToLogDifferences : public atom::error::Exception {
public:
    using Exception::Exception;
};

#define THROW_FAIL_TO_LOG_DIFFERENCES(...)                              \
    throw lithium::FailToLogDifferences(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                        ATOM_FUNC_NAME, __VA_ARGS__)

#define THROW_NESTED_FAIL_TO_LOG_DIFFERENCES(...) \
    lithium::FailToLogDifferences::rethrowNested( \
        ATOM_FILE_NAME, ATOM_FILE_LINE, ATOM_FUNC_NAME, __VA_ARGS__)

class FailToRecoverFiles : public atom::error::Exception {
public:
    using Exception::Exception;
};

#define THROW_FAIL_TO_RECOVER_FILES(...)                              \
    throw lithium::FailToRecoverFiles(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                      ATOM_FUNC_NAME, __VA_ARGS__)

#define THROW_NESTED_FAIL_TO_RECOVER_FILES(...)                                \
    lithium::FailToRecoverFiles::rethrowNested(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                               ATOM_FUNC_NAME, __VA_ARGS__)

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

    // Thread pool members
    std::vector<std::thread> threadPool;
    std::queue<std::function<void()>> tasks;
    std::mutex queueMutex;
    std::condition_variable condition;
    bool stop;

    // 新增成员
    std::atomic<bool> watching{false};
    std::thread watchThread;
    std::function<void(const fs::path&, const std::string&)> changeCallback;
    std::unordered_map<std::string, std::chrono::system_clock::time_point>
        fileCache;
    size_t maxCacheSize{1000};
    bool cacheEnabled{false};
    FileStats stats;

    Impl(std::string_view dir, std::string_view jFilePath,
         std::span<const std::string> types, bool rec)
        : directory(dir),
          jsonFilePath(jFilePath),
          recursive(rec),
          fileTypes(types.begin(), types.end()),
          stop(false) {
        // Initialize thread pool with hardware concurrency
        size_t threadCount = std::thread::hardware_concurrency();
        for (size_t i = 0; i < threadCount; ++i) {
            threadPool.emplace_back([this]() {
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(queueMutex);
                        condition.wait(
                            lock, [this]() { return stop || !tasks.empty(); });
                        if (stop && tasks.empty())
                            return;
                        task = std::move(tasks.front());
                        tasks.pop();
                    }
                    try {
                        task();
                    } catch (const std::exception& e) {
                        // Log or handle task exceptions
                        // For simplicity, we'll ignore here
                    }
                }
            });
        }
    }

    ~Impl() {
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            stop = true;
        }
        condition.notify_all();
        for (std::thread& thread : threadPool) {
            if (thread.joinable()) {
                thread.join();
            }
        }
    }

    void enqueueTask(std::function<void()> task) {
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            tasks.emplace(std::move(task));
        }
        condition.notify_one();
    }

    static void saveJSON(const json& j, const std::string& filePath,
                         const std::optional<std::string>& key) {
        std::ofstream outFile(filePath, std::ios::binary);
        if (!outFile.is_open()) {
            THROW_FAIL_TO_OPEN_FILE("Failed to open file for writing: " +
                                    filePath);
        }
        if (key) {
            std::vector<unsigned char> iv;
            std::vector<unsigned char> tag;
            std::string encrypted =
                atom::utils::encryptAES(j.dump(), *key, iv, tag);
            outFile.write(encrypted.data(), encrypted.size());
        } else {
            outFile << std::setw(4) << j << std::endl;
        }
    }

    static auto loadJSON(const std::string& filePath,
                         const std::optional<std::string>& key) -> json {
        std::ifstream inFile(filePath, std::ios::binary);
        if (!inFile.is_open()) {
            return {};
        }
        if (key) {
            std::string encrypted((std::istreambuf_iterator<char>(inFile)),
                                  std::istreambuf_iterator<char>());
            std::vector<unsigned char> iv;
            std::vector<unsigned char> tag;
            std::string decrypted =
                atom::utils::decryptAES(encrypted, *key, iv, tag);
            return json::parse(decrypted);
        }
        json j;
        inFile >> j;
        return j;
    }

    void generateJSON() {
        using DirIterVariant =
            std::variant<std::filesystem::directory_iterator,
                         std::filesystem::recursive_directory_iterator>;

        DirIterVariant fileRange =
            recursive
                ? DirIterVariant(
                      std::filesystem::recursive_directory_iterator(directory))
                : DirIterVariant(
                      std::filesystem::directory_iterator(directory));

        std::visit(
            [&](auto&& iter) {
                for (const auto& entry : iter) {
                    if (std::ranges::find(fileTypes,
                                          entry.path().extension().string()) !=
                        fileTypes.end()) {
                        enqueueTask(
                            [this, entry]() { processFile(entry.path()); });
                    }
                }
            },
            fileRange);

        // Wait for all tasks to finish
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            condition.wait(lock, [this]() { return tasks.empty(); });
        }

        saveJSON(newJson, jsonFilePath, encryptionKey);
    }

    void processFile(const std::filesystem::path& entry) {
        try {
            std::string hash = atom::utils::calculateSha256(entry.string());
            std::string lastWriteTime = atom::utils::getChinaTimestampString();

            std::unique_lock lock(mtx);
            newJson[entry.string()] = {
                {"last_write_time", lastWriteTime},
                {"hash", hash},
                {"size", std::filesystem::file_size(entry)},
                {"type", entry.extension().string()}};
        } catch (const std::exception& e) {
            // Handle file processing exceptions
            // For simplicity, we'll ignore here
        }
    }

    auto compareJSON() -> json {
        json diff;
        for (const auto& [filePath, newFileInfo] : newJson.items()) {
            if (oldJson.contains(filePath)) {
                if (oldJson[filePath]["hash"] != newFileInfo["hash"]) {
                    // 使用 difflib 生成详细的差异
                    std::vector<std::string> oldLines =
                        atom::utils::splitString(oldJson[filePath].dump(),
                                                 '\n');
                    std::vector<std::string> newLines =
                        atom::utils::splitString(newFileInfo.dump(), '\n');
                    auto differences = atom::utils::Differ::unifiedDiff(
                        oldLines, newLines, "old", "new");
                    diff[filePath] = {{"status", "modified"},
                                      {"diff", differences}};
                }
            } else {
                diff[filePath] = {{"status", "new"}};
            }
        }
        for (const auto& [filePath, oldFileInfo] : oldJson.items()) {
            if (!newJson.contains(filePath)) {
                diff[filePath] = {{"status", "deleted"}};
            }
        }
        return diff;
    }

    void recoverFiles() {
        for (const auto& [filePath, fileInfo] : oldJson.items()) {
            if (!std::filesystem::exists(filePath)) {
                try {
                    std::ofstream outFile(filePath);
                    if (outFile.is_open()) {
                        outFile << "This file was recovered based on version: "
                                << fileInfo["last_write_time"] << std::endl;
                        outFile.close();
                    }
                } catch (const std::exception& e) {
                    // Handle recovery exceptions
                    // For simplicity, we'll ignore here
                }
            }
        }
    }

    void watchDirectory() {
        while (watching) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            for (const auto& entry :
                 fs::recursive_directory_iterator(directory)) {
                if (!entry.is_regular_file()) {
                    continue;
                }

                const auto& path = entry.path();
                auto lastWrite = fs::last_write_time(path);
                auto it = fileCache.find(path.string());

                if (it != fileCache.end()) {
                    auto cachedTime = it->second;
                    if (std::chrono::file_clock::to_sys(lastWrite) >
                        cachedTime) {
                        if (changeCallback) {
                            changeCallback(path, "modified");
                        }
                        fileCache[path.string()] =
                            std::chrono::file_clock::to_sys(lastWrite);
                    }
                } else {
                    if (changeCallback) {
                        changeCallback(path, "new");
                    }
                    if (cacheEnabled) {
                        manageCacheSize();
                        fileCache[path.string()] =
                            std::chrono::file_clock::to_sys(lastWrite);
                    }
                }
            }
        }
    }

    void manageCacheSize() {
        if (fileCache.size() >= maxCacheSize) {
            size_t toRemove = maxCacheSize / 5;
            std::vector<
                std::pair<std::string, std::chrono::system_clock::time_point>>
                entries(fileCache.begin(), fileCache.end());

            std::sort(entries.begin(), entries.end(),
                      [](const auto& a, const auto& b) {
                          return a.second < b.second;
                      });

            for (size_t i = 0; i < toRemove && i < entries.size(); ++i) {
                fileCache.erase(entries[i].first);
            }
        }
    }

    void updateStats() {
        stats.lastScanTime = std::chrono::system_clock::now();
        stats.totalFiles = newJson.size();
        stats.modifiedFiles = 0;
        stats.newFiles = 0;
        stats.deletedFiles = 0;

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
    } catch (const std::exception& e) {
        // Handle scan exceptions
        THROW_FAIL_TO_SCAN_DIRECTORY("Scan failed: " + std::string(e.what()));
    }
}

void FileTracker::compare() {
    try {
        impl_->differences = impl_->compareJSON();
        impl_->updateStats();  // 更新统计信息
    } catch (const std::exception& e) {
        // Handle compare exceptions
        THROW_FAIL_TO_COMPARE_JSON("Compare failed: " + std::string(e.what()));
    }
}

void FileTracker::logDifferences(std::string_view logFilePath) const {
    try {
        std::ofstream logFile(logFilePath.data(), std::ios_base::app);
        if (!logFile.is_open()) {
            THROW_FAIL_TO_OPEN_FILE("Failed to open log file: " +
                                    std::string(logFilePath));
        }
        for (const auto& [filePath, info] : impl_->differences.items()) {
            logFile << "File: " << filePath << ", Status: " << info["status"]
                    << std::endl;
            if (info.contains("diff")) {
                for (const auto& line : info["diff"]) {
                    logFile << line << std::endl;
                }
            }
        }
    } catch (const std::exception& e) {
        // Handle logging exceptions
        THROW_FAIL_TO_LOG_DIFFERENCES("Logging failed: " +
                                      std::string(e.what()));
    }
}

void FileTracker::recover(std::string_view jsonFilePath) {
    try {
        impl_->oldJson =
            impl_->loadJSON(jsonFilePath.data(), impl_->encryptionKey);
        impl_->recoverFiles();
    } catch (const std::exception& e) {
        // Handle recovery exceptions
        THROW_FAIL_TO_RECOVER_FILES("Recovery failed: " +
                                    std::string(e.what()));
    }
}

auto FileTracker::asyncScan() -> std::future<void> {
    return std::async(std::launch::async, [this]() { scan(); });
}

auto FileTracker::asyncCompare() -> std::future<void> {
    return std::async(std::launch::async, [this]() { compare(); });
}

auto FileTracker::getDifferences() const noexcept -> const json& {
    return impl_->differences;
}

auto FileTracker::getTrackedFileTypes() const noexcept
    -> const std::vector<std::string>& {
    return impl_->fileTypes;
}

template <std::invocable<const std::filesystem::path&> Func>
void FileTracker::forEachFile(Func&& func) const {
    try {
        using DirIterVariant =
            std::variant<std::filesystem::directory_iterator,
                         std::filesystem::recursive_directory_iterator>;

        DirIterVariant fileRange =
            impl_->recursive
                ? DirIterVariant(std::filesystem::recursive_directory_iterator(
                      impl_->directory))
                : DirIterVariant(
                      std::filesystem::directory_iterator(impl_->directory));

        std::visit(
            [&](auto&& iter) {
                for (const auto& entry : iter) {
                    if (std::ranges::find(impl_->fileTypes,
                                          entry.path().extension().string()) !=
                        impl_->fileTypes.end()) {
                        func(entry.path());
                    }
                }
            },
            fileRange);
    } catch (const std::exception& e) {
        // Handle forEachFile exceptions
        // For simplicity, we'll ignore here
    }
}

auto FileTracker::getFileInfo(const std::filesystem::path& filePath) const
    -> std::optional<json> {
    std::shared_lock lock(impl_->mtx);
    if (auto it = impl_->newJson.find(filePath.string());
        it != impl_->newJson.end()) {
        return *it;
    }
    return std::nullopt;
}

void FileTracker::addFileType(std::string_view fileType) {
    std::unique_lock lock(impl_->mtx);
    impl_->fileTypes.emplace_back(fileType);
}

void FileTracker::removeFileType(std::string_view fileType) {
    std::unique_lock lock(impl_->mtx);
    impl_->fileTypes.erase(
        std::remove(impl_->fileTypes.begin(), impl_->fileTypes.end(), fileType),
        impl_->fileTypes.end());
}

void FileTracker::setEncryptionKey(std::string_view key) {
    std::unique_lock lock(impl_->mtx);
    impl_->encryptionKey = std::string(key);
}

// Explicitly instantiate the template function to avoid linker errors
template void
FileTracker::forEachFile<std::function<void(const std::filesystem::path&)>>(
    std::function<void(const std::filesystem::path&)>&&) const;

void FileTracker::startWatching() {
    impl_->watching = true;
    impl_->watchThread = std::thread(&Impl::watchDirectory, impl_.get());
}

void FileTracker::stopWatching() {
    impl_->watching = false;
    if (impl_->watchThread.joinable()) {
        impl_->watchThread.join();
    }
}

void FileTracker::setChangeCallback(
    std::function<void(const fs::path&, const std::string&)> callback) {
    impl_->changeCallback = std::move(callback);
}

void FileTracker::batchProcess(const std::vector<fs::path>& files,
                               std::function<void(const fs::path&)> processor) {
    const size_t batchSize = 100;
    for (size_t i = 0; i < files.size(); i += batchSize) {
        size_t end = std::min(i + batchSize, files.size());
        std::vector<std::future<void>> futures;

        for (size_t j = i; j < end; ++j) {
            futures.push_back(std::async(
                std::launch::async,
                [&processor, file = files[j]]() { processor(file); }));
        }

        for (auto& future : futures) {
            future.wait();
        }
    }
}

auto timePointToString(const std::chrono::system_clock::time_point& tp)
    -> std::string {
    std::time_t time = std::chrono::system_clock::to_time_t(tp);
    std::tm tm;
    localtime_r(&time, &tm);  // Thread-safe version of localtime
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

auto FileTracker::getStatistics() const -> json {
    json stats;
    stats["total_files"] = impl_->stats.totalFiles;
    stats["modified_files"] = impl_->stats.modifiedFiles;
    stats["new_files"] = impl_->stats.newFiles;
    stats["deleted_files"] = impl_->stats.deletedFiles;
    stats["last_scan_time"] = timePointToString(impl_->stats.lastScanTime);
    stats["watching"] = impl_->watching.load();
    stats["cache_enabled"] = impl_->cacheEnabled;
    stats["cache_size"] = impl_->fileCache.size();
    return stats;
}

void FileTracker::enableCache(bool enable) {
    impl_->cacheEnabled = enable;
    if (!enable) {
        impl_->fileCache.clear();
    }
}

void FileTracker::setCacheSize(size_t maxSize) {
    impl_->maxCacheSize = maxSize;
    impl_->manageCacheSize();
}

FileTracker::FileStats FileTracker::getCurrentStats() const {
    return impl_->stats;
}
}  // namespace lithium
