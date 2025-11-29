/*
 * process_spawning_win32.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifdef _WIN32

#include "process_spawning.hpp"

#include <spdlog/spdlog.h>

#include <windows.h>

namespace lithium::isolated {

Result<int> ProcessSpawner::spawn(
    const std::filesystem::path& pythonPath,
    const std::filesystem::path& executorPath,
    const IsolationConfig& config,
    std::pair<int, int> subprocessFds) {

    auto [readFd, writeFd] = subprocessFds;

    // Build command line
    std::string cmdLine = "\"" + pythonPath.string() + "\" \"" +
                          executorPath.string() + "\" " +
                          std::to_string(readFd) + " " +
                          std::to_string(writeFd);

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    // Create process
    if (!CreateProcessA(
            nullptr,
            cmdLine.data(),
            nullptr,
            nullptr,
            TRUE,  // Inherit handles
            CREATE_NO_WINDOW,
            nullptr,
            config.workingDirectory.empty() ? nullptr :
                config.workingDirectory.string().c_str(),
            &si,
            &pi)) {
        spdlog::error("CreateProcess failed: {}", GetLastError());
        return std::unexpected(RunnerError::ProcessSpawnFailed);
    }

    CloseHandle(pi.hThread);
    // Note: pi.hProcess is stored externally via getProcessHandle mechanism

    spdlog::debug("Spawned Python process with PID {}", pi.dwProcessId);
    return static_cast<int>(pi.dwProcessId);
}

Result<int> ProcessSpawner::waitForProcess(int processId, int timeoutMs) {
    HANDLE process = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_INFORMATION,
                                 FALSE, static_cast<DWORD>(processId));
    if (!process) {
        return std::unexpected(RunnerError::ProcessCrashed);
    }

    DWORD waitResult = WaitForSingleObject(
        process, timeoutMs == 0 ? INFINITE : static_cast<DWORD>(timeoutMs));

    if (waitResult == WAIT_TIMEOUT) {
        CloseHandle(process);
        return std::unexpected(RunnerError::Timeout);
    }

    DWORD exitCode;
    GetExitCodeProcess(process, &exitCode);
    CloseHandle(process);

    return static_cast<int>(exitCode);
}

Result<void> ProcessSpawner::killProcess(int processId) {
    HANDLE process = OpenProcess(PROCESS_TERMINATE, FALSE,
                                 static_cast<DWORD>(processId));
    if (!process) {
        return std::unexpected(RunnerError::ProcessCrashed);
    }

    if (!TerminateProcess(process, 1)) {
        CloseHandle(process);
        return std::unexpected(RunnerError::ProcessKilled);
    }

    CloseHandle(process);
    return {};
}

bool ProcessSpawner::isProcessRunning(int processId) {
    HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE,
                                 static_cast<DWORD>(processId));
    if (!process) {
        return false;
    }

    DWORD exitCode;
    if (GetExitCodeProcess(process, &exitCode)) {
        CloseHandle(process);
        return exitCode == STILL_ACTIVE;
    }

    CloseHandle(process);
    return false;
}

void* ProcessSpawner::getProcessHandle(int processId) {
    return OpenProcess(PROCESS_ALL_ACCESS, FALSE, static_cast<DWORD>(processId));
}

void ProcessSpawner::closeProcessHandle(void* handle) {
    if (handle) {
        CloseHandle(static_cast<HANDLE>(handle));
    }
}

}  // namespace lithium::isolated

#endif  // _WIN32
