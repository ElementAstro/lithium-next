#ifndef LITHIUM_MODULE_IMAGE_BASE_MTIME_HPP_
#define LITHIUM_MODULE_IMAGE_BASE_MTIME_HPP_

#include <chrono>
#include <filesystem>

std::chrono::system_clock::time_point get_file_mtime(
    const std::filesystem::path& path);

std::filesystem::file_time_type get_file_mtime(const std::string& filepath);

#endif  // LITHIUM_MODULE_IMAGE_BASE_MTIME_HPP_
