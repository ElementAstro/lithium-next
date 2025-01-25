#ifndef LITHIUM_ADDON_DEPENDENCY_MANAGER_HPP
#define LITHIUM_ADDON_DEPENDENCY_MANAGER_HPP

#include <exception>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace lithium {

class DependencyException : public std::exception {
public:
    explicit DependencyException(std::string message)
        : message_(std::move(message)) {}
    [[nodiscard]] auto what() const noexcept -> const char* override {
        return message_.c_str();
    }

private:
    std::string message_;
};

// 增强的错误类型
enum class DependencyErrorCode {
    SUCCESS = 0,
    PACKAGE_MANAGER_NOT_FOUND,
    INSTALL_FAILED,
    UNINSTALL_FAILED,
    DEPENDENCY_NOT_FOUND,
    CONFIG_LOAD_FAILED,
    INVALID_VERSION,
    NETWORK_ERROR,
    PERMISSION_DENIED,
    UNKNOWN_ERROR
};

class DependencyError {
public:
    DependencyError(DependencyErrorCode code, std::string message)
        : code_(code), message_(std::move(message)) {}

    [[nodiscard]] auto code() const -> DependencyErrorCode { return code_; }
    [[nodiscard]] auto message() const -> const std::string& {
        return message_;
    }

private:
    DependencyErrorCode code_;
    std::string message_;
};

// 异步操作结果
template <typename T>
struct DependencyResult {
    std::optional<T> value;
    std::optional<DependencyError> error;
};

// 版本信息结构
struct VersionInfo {
    int major{0};
    int minor{0};
    int patch{0};
    std::string prerelease;
};

// 增强的依赖项信息
struct DependencyInfo {
    std::string name;
    VersionInfo version;
    std::string packageManager;
    std::vector<std::string> dependencies;  // 新增：依赖关系
    bool optional{false};                   // 新增：可选依赖
    std::string minVersion;                 // 新增：最小版本要求
    std::string maxVersion;                 // 新增：最大版本要求
};

// 包管理器信息结构
struct PackageManagerInfo {
    std::string name;
    std::function<std::string(const DependencyInfo&)> getCheckCommand;
    std::function<std::string(const DependencyInfo&)> getInstallCommand;
    std::function<std::string(const DependencyInfo&)> getUninstallCommand;
    std::function<std::string(const std::string&)> getSearchCommand;
};

// 依赖管理器类
class DependencyManager {
public:
    explicit DependencyManager(
        const std::string& configPath = "package_managers.json");
    ~DependencyManager();

    DependencyManager(const DependencyManager&) = delete;
    DependencyManager& operator=(const DependencyManager&) = delete;

    // 简化的异步安装接口
    auto install(const std::string& name)
        -> std::future<DependencyResult<std::string>>;
    auto installWithVersion(const std::string& name, const std::string& version)
        -> std::future<DependencyResult<void>>;

    // 批量操作
    auto installMultiple(const std::vector<std::string>& deps)
        -> std::vector<std::future<DependencyResult<void>>>;

    // 新增：检查版本兼容性
    auto checkVersionCompatibility(const std::string& name,
                                   const std::string& version)
        -> DependencyResult<bool>;

    // 新增：获取依赖关系图
    auto getDependencyGraph() const -> std::string;

    // 新增：验证所有依赖的完整性
    auto verifyDependencies() -> std::future<DependencyResult<bool>>;

    // 新增：导出/导入依赖配置
    auto exportConfig() const -> DependencyResult<std::string>;
    auto importConfig(const std::string& config) -> DependencyResult<void>;

    void checkAndInstallDependencies();
    void setCustomInstallCommand(const std::string& dep,
                                 const std::string& command);
    auto generateDependencyReport() const -> std::string;
    void uninstallDependency(const std::string& dep);
    auto getCurrentPlatform() const -> std::string;
    void installDependencyAsync(const DependencyInfo& dep);
    void cancelInstallation(const std::string& dep);
    void addDependency(const DependencyInfo& dep);
    void removeDependency(const std::string& depName);
    auto searchDependency(const std::string& depName)
        -> std::vector<std::string>;
    void loadSystemPackageManagers();
    auto getPackageManagers() const -> std::vector<PackageManagerInfo>;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

}  // namespace lithium

#endif  // LITHIUM_ADDON_DEPENDENCY_MANAGER_HPP
