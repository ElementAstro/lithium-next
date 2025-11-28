#ifndef LITHIUM_SERVER_MIDDLEWARE_IMAGE_HPP
#define LITHIUM_SERVER_MIDDLEWARE_IMAGE_HPP

#include <memory>
#include "device/template/camera.hpp"
#include "usb.hpp"

#include "components/manager.hpp"
#include "config/config.hpp"
#include "device/manager.hpp"

#include "atom/async/message_bus.hpp"
#include "atom/function/global_ptr.hpp"
#include "atom/function/overload.hpp"
#include "atom/io/file_permission.hpp"
#include "atom/io/glob.hpp"
#include "atom/io/io.hpp"
#include "atom/log/spdlog_logger.hpp"
#include "atom/system/command.hpp"
#include "atom/system/env.hpp"
#include "atom/type/json.hpp"
#include "atom/utils/print.hpp"

#include "constant/constant.hpp"
#include "utils/macro.hpp"

namespace lithium::middleware {
struct ImageFiles {
    std::vector<std::string> captureFiles;
    std::vector<std::string> scheduleFiles;
};
namespace internal {
inline auto getAllFiles(const std::string& imageSaveBasePath) -> std::string {
    LOG_INFO( "getAllFiles: Starting file collection from {}",
          imageSaveBasePath);
    try {
        const auto capturePath = fs::path(imageSaveBasePath) / "CaptureImage";
        const auto planPath = fs::path(imageSaveBasePath) / "ScheduleImage";

        // 验证目录是否存在
        if (!atom::io::isFolderExists(capturePath.string()) ||
            !atom::io::isFolderExists(planPath.string())) {
            LOG_ERROR( "getAllFiles: Required directories do not exist");
            return "CaptureImage{}:ScheduleImage{}";
        }

        // 使用 glob 获取文件列表
        std::string captureString = "CaptureImage{";
        for (const auto& path : atom::meta::overload_cast<const std::string&>(
                 atom::io::glob)(capturePath / "*")) {
            if (atom::io::checkPathType(path) ==
                atom::io::PathType::REGULAR_FILE) {
                LOG_DEBUG( "getAllFiles: Found capture file: {}",
                      path.filename().string());
                captureString += path.filename().string() + ";";
            }
        }

        std::string planString = "ScheduleImage{";
        for (const auto& path : atom::meta::overload_cast<const std::string&>(
                 atom::io::glob)(planPath / "*")) {
            if (atom::io::checkPathType(path) ==
                atom::io::PathType::DIRECTORY) {
                LOG_DEBUG( "getAllFiles: Found schedule directory: {}",
                      path.filename().string());
                planString += path.filename().string() + ";";
            }
        }

        const auto resultString = captureString + "}:" + planString + '}';
        LOG_INFO( "getAllFiles: Successfully collected files");
        LOG_DEBUG( "getAllFiles: Result={}", resultString);

        return resultString;
    } catch (const std::exception& e) {
        LOG_ERROR( "getAllFiles: Error occurred while collecting files: {}",
              e.what());
        return "CaptureImage{}:ScheduleImage{}";
    }
}

inline auto parseString(const std::string& input,
                        const std::string& imgFilePath)
    -> std::vector<std::string> {
    std::vector<std::string> paths;
    std::string baseString;

    // 查找第一个'{'
    size_t pos = input.find('{');
    if (pos != std::string::npos) {
        // 获取 baseString
        baseString = input.substr(0, pos);

        // 获取 '{' 后的内容
        std::string content = input.substr(pos + 1);

        // 查找配对的 '}'
        size_t endPos = content.find('}');
        if (endPos != std::string::npos) {
            content = content.substr(0, endPos);

            // 去掉末尾的分号（如果有的话）
            if (!content.empty() && content.back() == ';') {
                content.pop_back();
            }

            // 分割 content
            size_t start = 0;
            size_t end;
            while ((end = content.find(';', start)) != std::string::npos) {
                std::string part = content.substr(start, end - start);
                // 去掉可能的空部分
                if (!part.empty()) {
                    // 拼接完整的路径并添加到路径列表
                    paths.push_back(std::filesystem::path(imgFilePath) /
                                    baseString / part);
                }
                start = end + 1;
            }
            // 添加最后一个部分（如果存在）
            if (start < content.size()) {
                std::string part = content.substr(start);
                if (!part.empty()) {
                    paths.push_back(std::filesystem::path(imgFilePath) /
                                    baseString / part);
                }
            }
        }
    }
    return paths;
}

inline std::string escapeSpecialChars(const std::string& input) {
    std::string result;
    for (char c : input) {
        switch (c) {
            case ' ':
                result += "\\ ";  // 转义空格
                break;
            case '[':
                result += "\\[";  // 转义左方括号
                break;
            case ']':
                result += "\\]";  // 转义右方括号
                break;
            case ',':
                result += "\\,";  // 转义逗号
                break;
            default:
                result += c;  // 保留其他字符
                break;
        }
    }
    return result;
}
}  // namespace internal

inline void showAllImageFolder() {
    LITHIUM_GET_REQUIRED_PTR(configManager, lithium::ConfigManager,
                             Constants::CONFIG_MANAGER);
    LITHIUM_GET_REQUIRED_PTR(messageBus, atom::async::MessageBus,
                             Constants::MESSAGE_BUS);

    auto imageSaveBasePathOpt =
        configManager->get("/quarcs/image/saveBasePath");
    auto imageSaveBasePath =
        imageSaveBasePathOpt.value_or("image").get<std::string>();

    auto result = internal::getAllFiles(imageSaveBasePath);
    LOG_INFO( "showAllImageFolder: Result={}", result);
    messageBus->publish("quarcs", "ShowAllImageFolder:{}"_fmt(result));
}

inline void moveImageToUSB(const std::string& path) {
    LOG_INFO( "moveImageToUSB: Entering function with path: {}", path);

    std::shared_ptr<ConfigManager> configManager;
    GET_OR_CREATE_PTR(configManager, ConfigManager, Constants::CONFIG_MANAGER)
    std::string imageBasePath =
        configManager->get("/quarcs/image/saveBasePath").value_or("~/images");
    std::vector<std::string> files = internal::parseString(path, imageBasePath);

    std::shared_ptr<atom::utils::Env> env;
    GET_OR_CREATE_PTR(env, atom::utils::Env, Constants::ENVIRONMENT)
    std::string basePath = "/media/" + env->getEnv("USER");

    // 验证基础路径
    if (!fs::exists(basePath)) {
        LOG_ERROR( "moveImageToUSB: Base directory does not exist: {}",
              basePath);
        return;
    }

    // 检测USB设备
    std::vector<std::string> folderList;
    for (const auto& entry : fs::directory_iterator(basePath)) {
        if (entry.is_directory() && entry.path().filename() != "CDROM") {
            LOG_DEBUG( "moveImageToUSB: Found device: {}",
                  entry.path().string());
            folderList.push_back(entry.path().string());
        }
    }

    if (folderList.empty()) {
        LOG_ERROR( "moveImageToUSB: No USB device found");
        return;
    }
    if (folderList.size() > 1) {
        LOG_WARN(
              "moveImageToUSB: Multiple USB devices found, using first one");
    }

    std::string usbMountPoint = folderList.front();
    LOG_INFO( "moveImageToUSB: Selected USB mount point: {}", usbMountPoint);

    const std::string PASSWORD = "quarcs";

    // 验证USB状态
    if (!fs::exists(usbMountPoint) || !fs::is_directory(usbMountPoint)) {
        LOG_ERROR( "moveImageToUSB: Invalid USB filesystem or not ready: {}",
              usbMountPoint);
        return;
    }

    // 检查权限并重新挂载
    if ((fs::status(usbMountPoint).permissions() & fs::perms::owner_write) !=
        fs::perms::none) {
        LOG_DEBUG(
              "moveImageToUSB: Attempting to remount filesystem as read-write");
        if (!internal::remountReadWrite(usbMountPoint, PASSWORD)) {
            LOG_ERROR(
                  "moveImageToUSB: Failed to remount filesystem as read-write");
            return;
        }
        LOG_INFO( "moveImageToUSB: Filesystem remounted as read-write");
    }

    // 检查空间
    long long remainingSpace = internal::getUSBSpace(usbMountPoint);
    if (remainingSpace == -1) {
        LOG_ERROR( "moveImageToUSB: Failed to get USB space for: {}",
              usbMountPoint);
        return;
    }

    long long totalSize = internal::getTotalSize(files);
    if (totalSize >= remainingSpace) {
        LOG_ERROR(
              "moveImageToUSB: Insufficient space on USB drive (need: {}, "
              "available: {})",
              totalSize, remainingSpace);
        return;
    }

    // 复制文件
    std::string folderPath = usbMountPoint + "/QUARCS_ImageSave";
    int sumMoveImage = 0;

    for (const auto& imgPath : files) {
        fs::path sourcePath(imgPath);
        fs::path destinationPath = fs::path(folderPath) / sourcePath.filename();
        LOG_DEBUG( "moveImageToUSB: Processing file: {} -> {}",
              sourcePath.string(), destinationPath.string());

        try {
            const auto mkdirCommand =
                "mkdir -p " + destinationPath.parent_path().string();
            if (!atom::system::executeCommandWithInput(mkdirCommand, PASSWORD)
                     .empty()) {
                LOG_ERROR( "moveImageToUSB: Failed to create directory: {}",
                      destinationPath.parent_path().string());
                continue;
            }

            const auto cpCommand =
                "cp -r " + sourcePath.string() + " " + destinationPath.string();
            if (!atom::system::executeCommandWithInput(cpCommand, PASSWORD)
                     .empty()) {
                LOG_ERROR( "moveImageToUSB: Failed to copy file: {} to {}",
                      sourcePath.string(), destinationPath.string());
                continue;
            }

            LOG_INFO( "moveImageToUSB: Successfully copied: {}",
                  sourcePath.filename().string());
            sumMoveImage++;
        } catch (const std::exception& e) {
            LOG_ERROR( "moveImageToUSB: Command execution failed: {}",
                  e.what());
            continue;
        }
    }

    LOG_INFO( "moveImageToUSB: Operation completed. Total files moved: {}",
          sumMoveImage);
}

inline void deleteFile(const std::string& path) {
    LOG_INFO( "deleteFile: Entering function with path={}", path);

    std::shared_ptr<ConfigManager> configManager;
    GET_OR_CREATE_PTR(configManager, ConfigManager, Constants::CONFIG_MANAGER)

    std::string imageBasePath =
        configManager->get("/quarcs/image/saveBasePath").value_or("~/images");
    std::vector<std::string> files = internal::parseString(path, imageBasePath);

    for (const auto& file : files) {
        LOG_DEBUG( "deleteFile: Processing file={}", file);

        auto opt = atom::io::compareFileAndSelfPermissions(file);
        if (!opt) {
            LOG_ERROR( "deleteFile: Failed to compare file permissions");
            continue;
        }

        std::string command;
        if (!opt.value()) {
            auto passwordOpt = configManager->get("/quarcs/password");
            if (!passwordOpt) {
                LOG_ERROR( "deleteFile: Failed to get password from config");
                continue;
            }
            LOG_WARN(
                  "deleteFile: Elevated permissions required for file={}",
                  file);
            command = "rm -rf {}"_fmt(file);
            auto result = atom::system::executeCommandWithInput(
                command, passwordOpt.value());
            if (!result.empty()) {
                LOG_ERROR( "deleteFile: Failed to delete file with sudo: {}",
                      result);
                continue;
            }
        } else {
            LOG_DEBUG(
                  "deleteFile: User has sufficient permissions for file={}",
                  file);
            command = "rm -rf {}"_fmt(file);
            auto result = atom::system::executeCommand(command);
            if (!result.empty()) {
                LOG_ERROR( "deleteFile: Failed to delete file: {}", result);
                continue;
            }
        }

        LOG_INFO( "deleteFile: Successfully deleted file={}", file);
    }

    LOG_INFO( "deleteFile: Exiting function");
}

inline void readImageFile(const std::string& message) {
    LOG_INFO( "Starting readImageFile operation with message: {}", message);
    auto imagePath = message;

    // 处理路径
    size_t pos = imagePath.find("ReadImageFile:");
    if (pos != std::string::npos) {
        LOG_DEBUG( "Replacing 'ReadImageFile:' with 'image/' in path");
        imagePath.replace(pos, 14, "image/");
    }
    LOG_DEBUG( "Processed image path: {}", imagePath);

    imagePath = internal::escapeSpecialChars(imagePath);
    LOG_DEBUG( "Escaped image path: {}", imagePath);

    // 获取必要组件
    LOG_DEBUG( "Retrieving required components");
    LITHIUM_GET_REQUIRED_PTR(componentManager, lithium::ComponentManager,
                             Constants::COMPONENT_MANAGER);
    LITHIUM_GET_REQUIRED_PTR(messageBus, atom::async::MessageBus,
                             Constants::MESSAGE_BUS);
    LITHIUM_GET_REQUIRED_PTR(configManager, lithium::ConfigManager,
                             Constants::CONFIG_MANAGER);
    LITHIUM_GET_REQUIRED_PTR(deviceManager, lithium::DeviceManager,
                             Constants::DEVICE_MANAGER);

    // 获取配置
    std::string vueImagePath = configManager->get("/quarcs/image/vueImagePath")
                                   .value_or("image")
                                   .get<std::string>();
    LOG_DEBUG( "Vue image path from config: {}", vueImagePath);

    // 获取相机信息
    auto camera = std::dynamic_pointer_cast<AtomCamera>(
        deviceManager->getPrimaryDevice("Camera"));
    if (!camera) {
        LOG_ERROR( "Failed to get primary camera device");
        return;
    }

    auto cameraBin = camera->getBinning();
    auto isColor = camera->isColor();
    auto processBin = false;
    LOG_DEBUG( "Camera settings - Binning: {}, IsColor: {}, ProcessBin: {}",
          cameraBin, isColor, processBin);

    // 检查组件
    if (!componentManager->hasComponent("lithium_image")) {
        LOG_ERROR( "Component 'lithium_image' not found");
        return;
    }
    LOG_DEBUG( "Found lithium_image component");

    try {
        LOG_INFO( "Attempting to save FITS as PNG");
        auto component = componentManager->getComponent("lithium_image")
                            .value()
                            .lock();
        auto result = std::any_cast<int>(
            component->dispatch("save_fits_as_png", imagePath, isColor, cameraBin,
                              processBin, vueImagePath));

        if (result == -1) {
            LOG_ERROR( "Failed to save FITS as PNG - Operation returned -1");
            return;
        }
        LOG_INFO( "Successfully saved FITS as PNG");
    } catch (const std::bad_any_cast& e) {
        LOG_ERROR( "Component cast failed: {}", e.what());
        return;
    } catch (const std::exception& e) {
        LOG_ERROR( "Unexpected error while saving FITS: {}", e.what());
        return;
    }

    LOG_INFO( "readImageFile operation completed successfully");
}
}  // namespace lithium::middleware

#endif  // LITHIUM_SERVER_MIDDLEWARE_IMAGE_HPP
