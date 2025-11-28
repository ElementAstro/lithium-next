#ifndef LITHIUM_SERVER_MIDDLEWARE_USB_HPP
#define LITHIUM_SERVER_MIDDLEWARE_USB_HPP

#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

#include "atom/async/message_bus.hpp"
#include "atom/function/global_ptr.hpp"
#include "atom/log/spdlog_logger.hpp"
#include "atom/sysinfo/disk.hpp"
#include "atom/system/env.hpp"

#include "constant/constant.hpp"
#include "utils/macro.hpp"

namespace fs = std::filesystem;

namespace lithium::middleware {
namespace internal {
inline bool remountReadWrite(const std::string& mountPoint,
                             const std::string& password) {
    std::ostringstream commandStream;
    commandStream << "echo '" << password << "' | sudo -S mount -o remount,rw "
                  << mountPoint;
    std::string command = commandStream.str();
    return system(command.c_str()) == 0;
}

inline long long getUSBSpace(const std::string& path) {
    try {
        auto spaceInfo = fs::space(path);
        return spaceInfo.available;
    } catch (const fs::filesystem_error& e) {
        LOG_ERROR( "getUSBSpace: Filesystem error: {}", e.what());
        return -1;
    }
}

inline long long getTotalSize(const std::vector<std::string>& paths) {
    long long totalSize = 0;
    for (const auto& path : paths) {
        try {
            totalSize += fs::file_size(path);
        } catch (const fs::filesystem_error& e) {
            LOG_ERROR( "getTotalSize: Filesystem error: {}", e.what());
        }
    }
    return totalSize;
}
}  // namespace internal
inline void usbCheck() {
    LOG_INFO( "usbCheck: Entering function");

    std::string base = "/media/";
    LITHIUM_GET_REQUIRED_PTR(env, atom::utils::Env, Constants::ENVIRONMENT)
    LITHIUM_GET_REQUIRED_PTR(messageBus, atom::async::MessageBus,
                             Constants::MESSAGE_BUS)

    const fs::path basePath = base + env->getEnv("USER");
    LOG_DEBUG( "usbCheck: Checking base path: {}", basePath.string());

    if (!fs::exists(basePath)) {
        LOG_ERROR( "usbCheck: Base directory does not exist: {}",
              basePath.string());
        return;
    }

    std::vector<std::string> folderList;
    for (const auto& entry : fs::directory_iterator(basePath)) {
        if (entry.is_directory() && entry.path().filename() != "CDROM") {
            const auto dirname = entry.path().filename().string();
            LOG_DEBUG( "usbCheck: Found directory: {}", dirname);
            folderList.push_back(dirname);
        }
    }

    if (folderList.size() == 1) {
        const fs::path usbMountPoint = basePath / folderList.at(0);
        const std::string& usbName = folderList.at(0);
        LOG_INFO( "usbCheck: Found single USB drive: {}", usbName);

        auto disks = atom::system::getDiskUsage();
        auto it = std::find_if(disks.begin(), disks.end(),
                               [&usbMountPoint](const auto& pair) {
                                   return pair.first == usbMountPoint.string();
                               });

        if (it == disks.end()) {
            LOG_ERROR( "usbCheck: Failed to get space info for USB drive: {}",
                  usbMountPoint.string());
            return;
        }

        const long long remainingSpace = it->second;
        const std::string message =
            "USBCheck:" + usbName + "," + std::to_string(remainingSpace);
        LOG_DEBUG( "usbCheck: Publishing message: {}", message);
        messageBus->publish("quarcs", message);

    } else if (folderList.empty()) {
        LOG_INFO( "usbCheck: No USB drive found");
        messageBus->publish("quarcs", "USBCheck:Null,Null");

    } else {
        LOG_WARN( "usbCheck: Multiple USB drives found: count={}",
              folderList.size());
        messageBus->publish("quarcs", "USBCheck:Multiple,Multiple");
    }

    LOG_INFO( "usbCheck: Exiting function");
}
}  // namespace lithium::middleware

#endif  // LITHIUM_SERVER_MIDDLEWARE_USB_HPP
