/*
 * process_utils.cpp
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "process_utils.hpp"

#include <array>
#include <cstdlib>
#include <sstream>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#endif

namespace lithium::venv {

CommandResult executeCommand(const std::string& command,
                             std::chrono::seconds timeout) {
    CommandResult result;

#ifdef _WIN32
    SECURITY_ATTRIBUTES saAttr;
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = nullptr;

    HANDLE hStdOutRead, hStdOutWrite;
    HANDLE hStdErrRead, hStdErrWrite;

    if (!CreatePipe(&hStdOutRead, &hStdOutWrite, &saAttr, 0) ||
        !CreatePipe(&hStdErrRead, &hStdErrWrite, &saAttr, 0)) {
        result.exitCode = -1;
        result.errorOutput = "Failed to create pipes";
        return result;
    }

    SetHandleInformation(hStdOutRead, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(hStdErrRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.hStdOutput = hStdOutWrite;
    si.hStdError = hStdErrWrite;
    si.dwFlags |= STARTF_USESTDHANDLES;
    ZeroMemory(&pi, sizeof(pi));

    std::string cmdLine = "cmd.exe /c " + command;

    if (!CreateProcessA(nullptr, cmdLine.data(), nullptr, nullptr, TRUE,
                        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        CloseHandle(hStdOutRead);
        CloseHandle(hStdOutWrite);
        CloseHandle(hStdErrRead);
        CloseHandle(hStdErrWrite);
        result.exitCode = -1;
        result.errorOutput = "Failed to create process";
        return result;
    }

    CloseHandle(hStdOutWrite);
    CloseHandle(hStdErrWrite);

    // Read output
    auto readPipe = [](HANDLE hPipe) -> std::string {
        std::string output;
        std::array<char, 4096> buffer;
        DWORD bytesRead;
        while (ReadFile(hPipe, buffer.data(), static_cast<DWORD>(buffer.size() - 1),
                        &bytesRead, nullptr) && bytesRead > 0) {
            buffer[bytesRead] = '\0';
            output += buffer.data();
        }
        return output;
    };

    std::thread stdoutThread([&]() { result.output = readPipe(hStdOutRead); });
    std::thread stderrThread([&]() { result.errorOutput = readPipe(hStdErrRead); });

    DWORD waitResult = WaitForSingleObject(pi.hProcess,
        static_cast<DWORD>(timeout.count() * 1000));

    if (waitResult == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 1);
        result.exitCode = -2;  // Timeout
    } else {
        DWORD exitCode;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        result.exitCode = static_cast<int>(exitCode);
    }

    stdoutThread.join();
    stderrThread.join();

    CloseHandle(hStdOutRead);
    CloseHandle(hStdErrRead);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

#else
    std::array<int, 2> stdoutPipe;
    std::array<int, 2> stderrPipe;

    if (pipe(stdoutPipe.data()) != 0 || pipe(stderrPipe.data()) != 0) {
        result.exitCode = -1;
        result.errorOutput = "Failed to create pipes";
        return result;
    }

    pid_t pid = fork();
    if (pid == -1) {
        result.exitCode = -1;
        result.errorOutput = "Failed to fork";
        return result;
    }

    if (pid == 0) {
        // Child process
        close(stdoutPipe[0]);
        close(stderrPipe[0]);
        dup2(stdoutPipe[1], STDOUT_FILENO);
        dup2(stderrPipe[1], STDERR_FILENO);
        close(stdoutPipe[1]);
        close(stderrPipe[1]);

        execl("/bin/sh", "sh", "-c", command.c_str(), nullptr);
        _exit(127);
    }

    // Parent process
    close(stdoutPipe[1]);
    close(stderrPipe[1]);

    auto readFd = [](int fd) -> std::string {
        std::string output;
        std::array<char, 4096> buffer;
        ssize_t bytesRead;
        while ((bytesRead = read(fd, buffer.data(), buffer.size() - 1)) > 0) {
            buffer[bytesRead] = '\0';
            output += buffer.data();
        }
        return output;
    };

    result.output = readFd(stdoutPipe[0]);
    result.errorOutput = readFd(stderrPipe[0]);

    close(stdoutPipe[0]);
    close(stderrPipe[0]);

    int status;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status)) {
        result.exitCode = WEXITSTATUS(status);
    } else {
        result.exitCode = -1;
    }
#endif

    return result;
}

}  // namespace lithium::venv
