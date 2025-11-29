/*
 * executor.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file executor.cpp
 * @brief Script executor factory implementation
 * @date 2024-1-13
 */

#include "executor.hpp"
#include "shell_executor.hpp"
#include "powershell_executor.hpp"
#include "python_executor.hpp"

#include <spdlog/spdlog.h>

namespace lithium::shell {

auto ScriptExecutorFactory::create(ScriptLanguage lang)
    -> std::unique_ptr<IScriptExecutor> {
    spdlog::debug("ScriptExecutorFactory: creating executor for language {}",
                  static_cast<int>(lang));

    switch (lang) {
        case ScriptLanguage::Shell:
            return std::make_unique<ShellExecutor>();
        case ScriptLanguage::PowerShell:
            return std::make_unique<PowerShellExecutor>();
        case ScriptLanguage::Python:
            return std::make_unique<PythonExecutor>();
        case ScriptLanguage::Auto:
            // Default to shell executor for auto-detection
            return std::make_unique<ShellExecutor>();
    }

    // Fallback to shell
    return std::make_unique<ShellExecutor>();
}

auto ScriptExecutorFactory::createForScript(std::string_view scriptContent)
    -> std::unique_ptr<IScriptExecutor> {
    ScriptLanguage lang = detectLanguage(scriptContent);
    return create(lang);
}

auto ScriptExecutorFactory::detectLanguage(std::string_view content)
    -> ScriptLanguage {
    std::string contentStr(content);

    // Check for Python indicators
    if (contentStr.find("#!/usr/bin/env python") != std::string::npos ||
        contentStr.find("#!/usr/bin/python") != std::string::npos ||
        (contentStr.find("import ") != std::string::npos &&
         contentStr.find("def ") != std::string::npos)) {
        return ScriptLanguage::Python;
    }

    // Check for PowerShell indicators
    if (contentStr.find("param(") != std::string::npos ||
        contentStr.find("$PSVersionTable") != std::string::npos ||
        contentStr.find("Write-Host") != std::string::npos ||
        contentStr.find("Get-") != std::string::npos) {
        return ScriptLanguage::PowerShell;
    }

    // Check for shell script indicators
    if (contentStr.find("#!/bin/bash") != std::string::npos ||
        contentStr.find("#!/bin/sh") != std::string::npos ||
        contentStr.find("#!/usr/bin/env bash") != std::string::npos) {
        return ScriptLanguage::Shell;
    }

    // Default to shell
    return ScriptLanguage::Shell;
}

}  // namespace lithium::shell
