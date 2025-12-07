/*
 * conda_adapter.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "conda_adapter.hpp"

#include <spdlog/spdlog.h>

#include <array>
#include <cstdlib>
#include <mutex>
#include <regex>
#include <sstream>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace lithium::venv {

namespace {

/**
 * @brief Command execution result
 */
struct CommandResult {
    int exitCode{-1};
    std::string output;
    std::string errorOutput;
};

/**
 * @brief Execute a command and capture output
 */
CommandResult executeCommand(
    const std::string& command,
    std::chrono::seconds timeout = std::chrono::seconds{300}) {
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
        while (ReadFile(hPipe, buffer.data(),
                        static_cast<DWORD>(buffer.size() - 1), &bytesRead,
                        nullptr) &&
               bytesRead > 0) {
            buffer[bytesRead] = '\0';
            output += buffer.data();
        }
        return output;
    };

    std::thread stdoutThread([&]() { result.output = readPipe(hStdOutRead); });
    std::thread stderrThread(
        [&]() { result.errorOutput = readPipe(hStdErrRead); });

    DWORD waitResult = WaitForSingleObject(
        pi.hProcess, static_cast<DWORD>(timeout.count() * 1000));

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

}  // anonymous namespace

// Implementation class
class CondaAdapter::Impl {
public:
    Impl() {
        spdlog::info("CondaAdapter initialized");
        detectConda();
    }

    ~Impl() { spdlog::info("CondaAdapter destroyed"); }

    void detectConda() {
#ifdef _WIN32
        auto result = executeCommand("where conda", std::chrono::seconds{10});
#else
        auto result = executeCommand("which conda", std::chrono::seconds{10});
#endif
        if (result.exitCode == 0 && !result.output.empty()) {
            std::string path = result.output;
            path.erase(path.find_last_not_of(" \r\n") + 1);
            condaPath_ = std::filesystem::path(path);
            condaAvailable_ = true;
            spdlog::info("Detected conda: {}", condaPath_.string());
        } else {
            condaAvailable_ = false;
        }
    }

    VenvResult<VenvInfo> createCondaEnv(const CondaEnvConfig& config,
                                        ProgressCallback callback) {
        if (!condaAvailable_) {
            return std::unexpected(VenvError::CondaNotFound);
        }

        if (callback) {
            callback(0.1f, "Creating conda environment...");
        }

        std::ostringstream cmd;
        cmd << "\"" << condaPath_.string() << "\" create -n " << config.name
            << " -y";

        if (!config.pythonVersion.empty()) {
            cmd << " python=" << config.pythonVersion;
        }

        for (const auto& channel : config.channels) {
            cmd << " -c " << channel;
        }

        for (const auto& pkg : config.packages) {
            cmd << " " << pkg;
        }

        spdlog::info("Creating conda env with command: {}", cmd.str());

        auto result = executeCommand(cmd.str(), operationTimeout_);
        if (result.exitCode != 0) {
            spdlog::error("Failed to create conda env: {}", result.errorOutput);
            return std::unexpected(VenvError::VenvCreationFailed);
        }

        if (callback) {
            callback(1.0f, "Complete");
        }

        return getCondaEnvInfo(config.name);
    }

    VenvResult<void> activateCondaEnv(std::string_view name) {
        if (!condaAvailable_) {
            return std::unexpected(VenvError::CondaNotFound);
        }

        // Get conda env path
        auto result =
            executeCommand("\"" + condaPath_.string() + "\" info --envs",
                           std::chrono::seconds{30});
        if (result.exitCode != 0) {
            return std::unexpected(VenvError::VenvActivationFailed);
        }

        // Find the environment path in the output
        std::regex envRegex(std::string(name) + R"(\s+(.+))");
        std::smatch match;
        if (std::regex_search(result.output, match, envRegex)) {
            std::string envPath = match[1].str();
            envPath.erase(envPath.find_last_not_of(" \r\n") + 1);
            activeVenvPath_ = std::filesystem::path(envPath);
            isVenvActive_ = true;
            activeCondaEnv_ = std::string(name);

#ifdef _WIN32
            _putenv_s("CONDA_PREFIX", envPath.c_str());
#else
            setenv("CONDA_PREFIX", envPath.c_str(), 1);
#endif
            spdlog::info("Activated conda environment: {}", name);
            return {};
        }

        return std::unexpected(VenvError::VenvNotFound);
    }

    VenvResult<void> deactivateCondaEnv() {
        std::lock_guard<std::mutex> lock(mutex_);

#ifdef _WIN32
        _putenv_s("CONDA_PREFIX", "");
#else
        unsetenv("CONDA_PREFIX");
#endif

        activeCondaEnv_.reset();
        activeVenvPath_.reset();
        isVenvActive_ = false;

        spdlog::info("Deactivated conda environment");
        return {};
    }

    VenvResult<void> deleteCondaEnv(std::string_view name) {
        if (!condaAvailable_) {
            return std::unexpected(VenvError::CondaNotFound);
        }

        std::ostringstream cmd;
        cmd << "\"" << condaPath_.string() << "\" env remove -n " << name
            << " -y";

        auto result = executeCommand(cmd.str(), operationTimeout_);
        if (result.exitCode != 0) {
            spdlog::error("Failed to delete conda env: {}", result.errorOutput);
            return std::unexpected(VenvError::PermissionDenied);
        }

        spdlog::info("Deleted conda environment: {}", name);
        return {};
    }

    VenvResult<std::vector<VenvInfo>> listCondaEnvs() const {
        if (!condaAvailable_) {
            return std::unexpected(VenvError::CondaNotFound);
        }

        auto result =
            executeCommand("\"" + condaPath_.string() + "\" info --envs",
                           std::chrono::seconds{30});
        if (result.exitCode != 0) {
            return std::unexpected(VenvError::UnknownError);
        }

        std::vector<VenvInfo> envs;
        std::istringstream iss(result.output);
        std::string line;

        // Skip header lines
        while (std::getline(iss, line)) {
            if (line.empty() || line[0] == '#')
                continue;

            // Parse environment line: name  path or *name  path (active)
            std::regex envRegex(R"((\*?)(\S+)\s+(.+))");
            std::smatch match;
            if (std::regex_match(line, match, envRegex)) {
                VenvInfo info;
                info.isActive = !match[1].str().empty();
                info.name = match[2].str();
                std::string path = match[3].str();
                path.erase(path.find_last_not_of(" \r\n") + 1);
                info.path = std::filesystem::path(path);
                info.isConda = true;
                envs.push_back(std::move(info));
            }
        }

        return envs;
    }

    VenvResult<VenvInfo> getCondaEnvInfo(std::string_view name) const {
        auto envsResult = listCondaEnvs();
        if (!envsResult) {
            return std::unexpected(envsResult.error());
        }

        for (const auto& env : *envsResult) {
            if (env.name == name) {
                return env;
            }
        }

        return std::unexpected(VenvError::VenvNotFound);
    }

    bool isCondaAvailable() const { return condaAvailable_; }

    void setCondaPath(const std::filesystem::path& condaPath) {
        condaPath_ = condaPath;
        condaAvailable_ = std::filesystem::exists(condaPath);
    }

    void setOperationTimeout(std::chrono::seconds timeout) {
        operationTimeout_ = timeout;
    }

private:
    mutable std::mutex mutex_;
    std::filesystem::path condaPath_;
    bool condaAvailable_{false};
    bool isVenvActive_{false};
    std::optional<std::filesystem::path> activeVenvPath_;
    std::optional<std::string> activeCondaEnv_;
    std::chrono::seconds operationTimeout_{300};
};

// CondaAdapter implementation forwarding to Impl

CondaAdapter::CondaAdapter() : pImpl_(std::make_unique<Impl>()) {}

CondaAdapter::~CondaAdapter() = default;

CondaAdapter::CondaAdapter(CondaAdapter&&) noexcept = default;

CondaAdapter& CondaAdapter::operator=(CondaAdapter&&) noexcept = default;

bool CondaAdapter::isCondaAvailable() const {
    return pImpl_->isCondaAvailable();
}

void CondaAdapter::detectConda() { pImpl_->detectConda(); }

void CondaAdapter::setCondaPath(const std::filesystem::path& condaPath) {
    pImpl_->setCondaPath(condaPath);
}

VenvResult<VenvInfo> CondaAdapter::createCondaEnv(const CondaEnvConfig& config,
                                                  ProgressCallback callback) {
    return pImpl_->createCondaEnv(config, std::move(callback));
}

VenvResult<VenvInfo> CondaAdapter::createCondaEnv(
    std::string_view name, std::string_view pythonVersion) {
    CondaEnvConfig config;
    config.name = std::string(name);
    if (!pythonVersion.empty()) {
        config.pythonVersion = std::string(pythonVersion);
    }
    return pImpl_->createCondaEnv(config, nullptr);
}

VenvResult<void> CondaAdapter::activateCondaEnv(std::string_view name) {
    return pImpl_->activateCondaEnv(name);
}

VenvResult<void> CondaAdapter::deactivateCondaEnv() {
    return pImpl_->deactivateCondaEnv();
}

VenvResult<void> CondaAdapter::deleteCondaEnv(std::string_view name) {
    return pImpl_->deleteCondaEnv(name);
}

VenvResult<std::vector<VenvInfo>> CondaAdapter::listCondaEnvs() const {
    return pImpl_->listCondaEnvs();
}

VenvResult<VenvInfo> CondaAdapter::getCondaEnvInfo(
    std::string_view name) const {
    return pImpl_->getCondaEnvInfo(name);
}

void CondaAdapter::setOperationTimeout(std::chrono::seconds timeout) {
    pImpl_->setOperationTimeout(timeout);
}

}  // namespace lithium::venv
