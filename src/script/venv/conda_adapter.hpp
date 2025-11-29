/*
 * conda_adapter.hpp
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef LITHIUM_SCRIPT_VENV_CONDA_ADAPTER_HPP
#define LITHIUM_SCRIPT_VENV_CONDA_ADAPTER_HPP

#include "types.hpp"
#include <filesystem>
#include <memory>

namespace lithium::venv {

/**
 * @brief Adapter for Conda environment management
 */
class CondaAdapter {
public:
    CondaAdapter();
    ~CondaAdapter();

    CondaAdapter(const CondaAdapter&) = delete;
    CondaAdapter& operator=(const CondaAdapter&) = delete;
    CondaAdapter(CondaAdapter&&) noexcept;
    CondaAdapter& operator=(CondaAdapter&&) noexcept;

    // Detection
    [[nodiscard]] bool isCondaAvailable() const;
    void detectConda();
    void setCondaPath(const std::filesystem::path& condaPath);

    // Environment operations
    [[nodiscard]] VenvResult<VenvInfo> createCondaEnv(
        const CondaEnvConfig& config,
        ProgressCallback callback = nullptr);

    [[nodiscard]] VenvResult<VenvInfo> createCondaEnv(
        std::string_view name,
        std::string_view pythonVersion = "");

    [[nodiscard]] VenvResult<void> activateCondaEnv(std::string_view name);
    [[nodiscard]] VenvResult<void> deactivateCondaEnv();
    [[nodiscard]] VenvResult<void> deleteCondaEnv(std::string_view name);

    [[nodiscard]] VenvResult<std::vector<VenvInfo>> listCondaEnvs() const;
    [[nodiscard]] VenvResult<VenvInfo> getCondaEnvInfo(std::string_view name) const;

    void setOperationTimeout(std::chrono::seconds timeout);

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

}  // namespace lithium::venv

#endif
