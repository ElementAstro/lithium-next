/*
 * resource_monitor.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "resource_monitor.hpp"

#include <fstream>
#include <string>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#else
#include <unistd.h>
#endif

namespace lithium::isolated {

std::optional<size_t> ResourceMonitor::getMemoryUsage(int processId) {
    if (processId <= 0) return std::nullopt;

#ifdef _WIN32
    HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
                                 FALSE, static_cast<DWORD>(processId));
    if (process) {
        PROCESS_MEMORY_COUNTERS pmc;
        if (GetProcessMemoryInfo(process, &pmc, sizeof(pmc))) {
            CloseHandle(process);
            return pmc.WorkingSetSize;
        }
        CloseHandle(process);
    }
#else
    std::ifstream statm("/proc/" + std::to_string(processId) + "/statm");
    if (statm) {
        size_t size;
        statm >> size;
        return size * sysconf(_SC_PAGESIZE);
    }
#endif
    return std::nullopt;
}

std::optional<double> ResourceMonitor::getCpuUsage(int processId) {
    // CPU usage monitoring is complex and platform-specific
    // For now, return nullopt (not implemented)
    // A full implementation would need to sample /proc/[pid]/stat over time
    (void)processId;
    return std::nullopt;
}

bool ResourceMonitor::isMemoryLimitExceeded(int processId, size_t limitMB) {
    if (limitMB == 0) return false;  // No limit

    auto memUsage = getMemoryUsage(processId);
    if (memUsage) {
        return *memUsage > limitMB * 1024 * 1024;
    }
    return false;
}

std::optional<size_t> ResourceMonitor::getPeakMemoryUsage(int processId) {
    if (processId <= 0) return std::nullopt;

#ifdef _WIN32
    HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
                                 FALSE, static_cast<DWORD>(processId));
    if (process) {
        PROCESS_MEMORY_COUNTERS pmc;
        if (GetProcessMemoryInfo(process, &pmc, sizeof(pmc))) {
            CloseHandle(process);
            return pmc.PeakWorkingSetSize;
        }
        CloseHandle(process);
    }
#else
    std::ifstream status("/proc/" + std::to_string(processId) + "/status");
    if (status) {
        std::string line;
        while (std::getline(status, line)) {
            if (line.compare(0, 6, "VmPeak") == 0) {
                size_t value;
                if (sscanf(line.c_str(), "VmPeak: %zu kB", &value) == 1) {
                    return value * 1024;  // Convert kB to bytes
                }
            }
        }
    }
#endif
    return std::nullopt;
}

}  // namespace lithium::isolated
