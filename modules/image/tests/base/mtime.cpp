
#include "base/mtime.hpp"

#include <gtest/gtest.h>
#include <chrono>
#include <filesystem>
#include <fstream>

#ifdef __linux__
#include <fcntl.h>
#endif

namespace fs = std::filesystem;
using namespace std::chrono;

class FileTimeTest : public ::testing::Test {
protected:
    fs::path test_dir;
    fs::path test_file;

    void SetUp() override {
        // 创建临时测试目录
        test_dir = fs::temp_directory_path() / "file_time_test";
        fs::create_directory(test_dir);
        test_file = test_dir / "test.txt";
    }

    void TearDown() override {
        // 清理测试目录
        fs::remove_all(test_dir);
    }

    void create_test_file(const fs::path& path,
                          system_clock::time_point tp = system_clock::now()) {
        // 创建测试文件
        std::ofstream ofs(path);
        ofs << "test content" << std::endl;
        ofs.close();

// 设置文件时间
#ifdef _WIN32
        HANDLE hFile = CreateFileW(path.c_str(), GENERIC_WRITE, 0, NULL,
                                   OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile != INVALID_HANDLE_VALUE) {
            auto tt = system_clock::to_time_t(tp);
            FILETIME ft;
            SYSTEMTIME st;
            GetSystemTime(&st);
            SystemTimeToFileTime(&st, &ft);
            SetFileTime(hFile, NULL, NULL, &ft);
            CloseHandle(hFile);
        }
#else
        struct timespec times[2];
        auto tt = system_clock::to_time_t(tp);
        times[0].tv_sec = tt;
        times[0].tv_nsec =
            duration_cast<nanoseconds>(tp.time_since_epoch()).count() %
            1000000000;
        times[1] = times[0];
        utimensat(AT_FDCWD, path.c_str(), times, 0);
#endif
    }
};

// 基本功能测试
TEST_F(FileTimeTest, BasicFileTime) {
    auto now = system_clock::now();
    create_test_file(test_file, now);

    auto mtime = get_file_mtime(test_file);
    auto diff = duration_cast<seconds>(mtime - now).count();

    EXPECT_LE(abs(diff), 1);  // 允许1秒误差
}

// 错误处理测试
TEST_F(FileTimeTest, NonExistentFile) {
    EXPECT_THROW(get_file_mtime(test_dir / "nonexistent.txt"),
                 std::system_error);
}

// 时间精度测试
TEST_F(FileTimeTest, TimeResolution) {
    auto now = system_clock::now();
    create_test_file(test_file, now);

    auto mtime = get_file_mtime(test_file);
    auto diff_ns = duration_cast<nanoseconds>(mtime - now).count();

#ifdef _WIN32
    EXPECT_LE(abs(diff_ns), 10000000);  // Windows精度100ns
#else
    EXPECT_LE(abs(diff_ns), 1000000000);  // Unix精度1s
#endif
}
