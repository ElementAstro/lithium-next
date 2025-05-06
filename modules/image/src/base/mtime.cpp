#include "mtime.hpp"
#include <loguru.hpp>
#include <system_error>

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

// 获取文件最后修改时间（跨平台实现）
std::chrono::system_clock::time_point get_file_mtime(
    const std::filesystem::path& path) {
    LOG_F(INFO, "Attempting to get file modification time for: %s",
          path.string().c_str());
#ifdef _WIN32
    // Windows实现
    WIN32_FILE_ATTRIBUTE_DATA fileAttrData;

    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard,
                              &fileAttrData)) {
        LOG_F(ERROR, "Failed to get file attributes for: %s",
              path.string().c_str());
        throw std::system_error(GetLastError(), std::system_category(),
                                "Failed to get file attributes");
    }

    ULARGE_INTEGER ull;
    ull.LowPart = fileAttrData.ftLastWriteTime.dwLowDateTime;
    ull.HighPart = fileAttrData.ftLastWriteTime.dwHighDateTime;

    // 将Windows文件时间转换为系统时钟时间
    constexpr uint64_t WINDOWS_TICK = 10000000;
    constexpr uint64_t SEC_TO_UNIX_EPOCH = 11644473600LL;

    uint64_t timestamp = ull.QuadPart / WINDOWS_TICK - SEC_TO_UNIX_EPOCH;
    LOG_F(INFO, "File modification time for %s is %llu", path.string().c_str(),
          timestamp);
    return std::chrono::system_clock::from_time_t(timestamp) +
           std::chrono::nanoseconds((ull.QuadPart % WINDOWS_TICK) * 100);

#else
    // POSIX实现（Linux/macOS）
    struct stat fileStat;
    if (stat(path.c_str(), &fileStat) != 0) {
        LOG_F(ERROR, "Failed to get file status for: %s",
              path.string().c_str());
        throw std::system_error(errno, std::generic_category(),
                                "Failed to get file status");
    }

#if defined(__APPLE__)
    // macOS的时间结构
    auto mod_time = fileStat.st_mtimespec;
#else
    // Linux的时间结构
    auto mod_time = fileStat.st_mtim;
#endif

    LOG_F(INFO,
          "File modification time for %s is %ld seconds and %ld nanoseconds",
          path.string().c_str(), mod_time.tv_sec, mod_time.tv_nsec);
    return std::chrono::system_clock::from_time_t(mod_time.tv_sec) +
           std::chrono::nanoseconds(mod_time.tv_nsec);
#endif
}

// 获取文件修改时间
std::filesystem::file_time_type get_file_mtime(const std::string& filepath) {
    LOG_F(INFO, "Attempting to get file modification time for: %s",
          filepath.c_str());
    std::error_code ec;
    auto status = std::filesystem::status(filepath, ec);

    if (ec) {
        LOG_F(ERROR, "Failed to get file status for: %s", filepath.c_str());
        throw std::system_error(ec);
    }

    // 检查文件是否存在且可访问
    if (!std::filesystem::exists(status)) {
        LOG_F(ERROR, "File does not exist: %s", filepath.c_str());
        throw std::system_error(
            std::make_error_code(std::errc::no_such_file_or_directory));
    }

#ifndef _WIN32
    // Unix系统下检查读取权限
    if (access(filepath.c_str(), R_OK) != 0) {
        LOG_F(ERROR, "No read access to file: %s", filepath.c_str());
        throw std::system_error(std::error_code(errno, std::system_category()));
    }
#endif

    auto file_time = std::filesystem::last_write_time(filepath, ec);
    if (ec) {
        LOG_F(ERROR, "Failed to get last write time for: %s", filepath.c_str());
        throw std::system_error(ec);
    }

    LOG_F(INFO, "Successfully retrieved file modification time for: %s",
          filepath.c_str());
    return file_time;
}
