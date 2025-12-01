/**
 * @file test_check_comprehensive.cpp
 * @brief Comprehensive unit tests for CommandChecker
 *
 * Tests for:
 * - Rule management (add, remove, list)
 * - Command checking
 * - Configuration (dangerous commands, line length, nesting depth)
 * - Security features (privileged commands, forbidden patterns, sandbox)
 * - Resource limits
 * - Typed rules with concepts
 * - JSON serialization
 * - Config file persistence
 * - Error printing
 */

#include <gtest/gtest.h>
#include <chrono>
#include <filesystem>
#include <fstream>

#include "debug/check.hpp"

namespace lithium::debug::test {

// ============================================================================
// ErrorSeverity Tests
// ============================================================================

class ErrorSeverityTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ErrorSeverityTest, SeverityValues) {
    EXPECT_NE(ErrorSeverity::WARNING, ErrorSeverity::ERROR);
    EXPECT_NE(ErrorSeverity::ERROR, ErrorSeverity::CRITICAL);
    EXPECT_NE(ErrorSeverity::WARNING, ErrorSeverity::CRITICAL);
}

// ============================================================================
// CommandChecker::Error Tests
// ============================================================================

class CommandCheckerErrorTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(CommandCheckerErrorTest, ErrorConstruction) {
    CommandChecker::Error error;
    error.message = "Test error";
    error.line = 1;
    error.column = 5;
    error.severity = ErrorSeverity::ERROR;

    EXPECT_EQ(error.message, "Test error");
    EXPECT_EQ(error.line, 1);
    EXPECT_EQ(error.column, 5);
    EXPECT_EQ(error.severity, ErrorSeverity::ERROR);
}

// ============================================================================
// CommandChecker Basic Tests
// ============================================================================

class CommandCheckerBasicTest : public ::testing::Test {
protected:
    void SetUp() override { checker = std::make_unique<CommandChecker>(); }

    void TearDown() override { checker.reset(); }

    std::unique_ptr<CommandChecker> checker;
};

TEST_F(CommandCheckerBasicTest, DefaultConstruction) {
    EXPECT_NE(checker, nullptr);
}

TEST_F(CommandCheckerBasicTest, CheckEmptyCommand) {
    auto errors = checker->check("");
    // Empty command should produce an error
    EXPECT_GE(errors.size(), 1);
}

TEST_F(CommandCheckerBasicTest, CheckValidCommand) {
    auto errors = checker->check("echo hello");
    // Valid simple command should have no errors
    EXPECT_EQ(errors.size(), 0);
}

TEST_F(CommandCheckerBasicTest, CheckMultilineCommand) {
    auto errors = checker->check("echo line1\necho line2\necho line3");
    // Multiple lines should be checked
}

// ============================================================================
// CommandChecker Rule Management Tests
// ============================================================================

class CommandCheckerRuleTest : public ::testing::Test {
protected:
    void SetUp() override { checker = std::make_unique<CommandChecker>(); }

    void TearDown() override { checker.reset(); }

    std::unique_ptr<CommandChecker> checker;
};

TEST_F(CommandCheckerRuleTest, AddRule) {
    checker->addRule("custom_rule",
                     [](const std::string& line,
                        size_t lineNum) -> std::optional<CommandChecker::Error> {
                         if (line.find("forbidden") != std::string::npos) {
                             return CommandChecker::Error{
                                 "Forbidden word detected", lineNum, 0,
                                 ErrorSeverity::ERROR};
                         }
                         return std::nullopt;
                     });

    auto rules = checker->listRules();
    bool found = false;
    for (const auto& rule : rules) {
        if (rule == "custom_rule") {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(CommandCheckerRuleTest, RemoveRule) {
    checker->addRule(
        "temp_rule",
        [](const std::string&, size_t) { return std::nullopt; });

    bool removed = checker->removeRule("temp_rule");
    EXPECT_TRUE(removed);

    auto rules = checker->listRules();
    bool found = false;
    for (const auto& rule : rules) {
        if (rule == "temp_rule") {
            found = true;
            break;
        }
    }
    EXPECT_FALSE(found);
}

TEST_F(CommandCheckerRuleTest, RemoveNonexistentRule) {
    bool removed = checker->removeRule("nonexistent_rule");
    EXPECT_FALSE(removed);
}

TEST_F(CommandCheckerRuleTest, ListRules) {
    auto rules = checker->listRules();
    // Should have default rules
    EXPECT_FALSE(rules.empty());
}

TEST_F(CommandCheckerRuleTest, CustomRuleTriggered) {
    checker->addRule("test_rule",
                     [](const std::string& line,
                        size_t lineNum) -> std::optional<CommandChecker::Error> {
                         if (line.find("trigger") != std::string::npos) {
                             return CommandChecker::Error{
                                 "Trigger word found", lineNum, 0,
                                 ErrorSeverity::WARNING};
                         }
                         return std::nullopt;
                     });

    auto errors = checker->check("echo trigger");
    bool found = false;
    for (const auto& error : errors) {
        if (error.message == "Trigger word found") {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

// ============================================================================
// CommandChecker Typed Rule Tests
// ============================================================================

// Custom rule type satisfying CheckRule concept
struct CustomTypedRule {
    bool check(const std::string& line) const {
        return line.find("bad") == std::string::npos;
    }

    ErrorSeverity severity() const { return ErrorSeverity::WARNING; }

    std::string message() const { return "Bad word detected"; }
};

class CommandCheckerTypedRuleTest : public ::testing::Test {
protected:
    void SetUp() override { checker = std::make_unique<CommandChecker>(); }

    void TearDown() override { checker.reset(); }

    std::unique_ptr<CommandChecker> checker;
};

TEST_F(CommandCheckerTypedRuleTest, AddTypedRule) {
    CustomTypedRule rule;
    checker->addTypedRule("typed_rule", rule);

    auto rules = checker->listRules();
    bool found = false;
    for (const auto& r : rules) {
        if (r == "typed_rule") {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(CommandCheckerTypedRuleTest, TypedRuleTriggered) {
    CustomTypedRule rule;
    checker->addTypedRule("bad_word_rule", rule);

    auto errors = checker->check("echo bad word");
    bool found = false;
    for (const auto& error : errors) {
        if (error.message == "Bad word detected") {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

// ============================================================================
// CommandChecker Dangerous Commands Tests
// ============================================================================

class CommandCheckerDangerousTest : public ::testing::Test {
protected:
    void SetUp() override { checker = std::make_unique<CommandChecker>(); }

    void TearDown() override { checker.reset(); }

    std::unique_ptr<CommandChecker> checker;
};

TEST_F(CommandCheckerDangerousTest, DefaultDangerousCommands) {
    auto errors = checker->check("rm -rf /");
    bool found = false;
    for (const auto& error : errors) {
        if (error.message.find("Dangerous command") != std::string::npos) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(CommandCheckerDangerousTest, SetDangerousCommands) {
    std::vector<std::string> customDangerous = {"danger1", "danger2"};
    checker->setDangerousCommands(customDangerous);

    auto errors = checker->check("danger1 something");
    bool found = false;
    for (const auto& error : errors) {
        if (error.message.find("danger1") != std::string::npos) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(CommandCheckerDangerousTest, MkfsDetected) {
    auto errors = checker->check("mkfs.ext4 /dev/sda1");
    bool found = false;
    for (const auto& error : errors) {
        if (error.message.find("mkfs") != std::string::npos) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(CommandCheckerDangerousTest, DdDetected) {
    auto errors = checker->check("dd if=/dev/zero of=/dev/sda");
    bool found = false;
    for (const auto& error : errors) {
        if (error.message.find("dd") != std::string::npos) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

// ============================================================================
// CommandChecker Line Length Tests
// ============================================================================

class CommandCheckerLineLengthTest : public ::testing::Test {
protected:
    void SetUp() override { checker = std::make_unique<CommandChecker>(); }

    void TearDown() override { checker.reset(); }

    std::unique_ptr<CommandChecker> checker;
};

TEST_F(CommandCheckerLineLengthTest, DefaultMaxLineLength) {
    std::string longLine(100, 'x');
    auto errors = checker->check(longLine);
    bool found = false;
    for (const auto& error : errors) {
        if (error.message.find("maximum length") != std::string::npos) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(CommandCheckerLineLengthTest, SetMaxLineLength) {
    checker->setMaxLineLength(200);

    std::string longLine(150, 'x');
    auto errors = checker->check(longLine);
    bool found = false;
    for (const auto& error : errors) {
        if (error.message.find("maximum length") != std::string::npos) {
            found = true;
            break;
        }
    }
    EXPECT_FALSE(found);  // Should not trigger with increased limit
}

TEST_F(CommandCheckerLineLengthTest, ShortLineNoError) {
    auto errors = checker->check("echo hello");
    bool found = false;
    for (const auto& error : errors) {
        if (error.message.find("maximum length") != std::string::npos) {
            found = true;
            break;
        }
    }
    EXPECT_FALSE(found);
}

// ============================================================================
// CommandChecker Quote Detection Tests
// ============================================================================

class CommandCheckerQuoteTest : public ::testing::Test {
protected:
    void SetUp() override { checker = std::make_unique<CommandChecker>(); }

    void TearDown() override { checker.reset(); }

    std::unique_ptr<CommandChecker> checker;
};

TEST_F(CommandCheckerQuoteTest, UnmatchedDoubleQuotes) {
    auto errors = checker->check("echo \"hello");
    bool found = false;
    for (const auto& error : errors) {
        if (error.message.find("double quotes") != std::string::npos) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(CommandCheckerQuoteTest, UnmatchedSingleQuotes) {
    auto errors = checker->check("echo 'hello");
    bool found = false;
    for (const auto& error : errors) {
        if (error.message.find("single quotes") != std::string::npos) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(CommandCheckerQuoteTest, MatchedQuotes) {
    auto errors = checker->check("echo \"hello world\"");
    bool found = false;
    for (const auto& error : errors) {
        if (error.message.find("quotes") != std::string::npos) {
            found = true;
            break;
        }
    }
    EXPECT_FALSE(found);
}

// ============================================================================
// CommandChecker Backtick Tests
// ============================================================================

class CommandCheckerBacktickTest : public ::testing::Test {
protected:
    void SetUp() override { checker = std::make_unique<CommandChecker>(); }

    void TearDown() override { checker.reset(); }

    std::unique_ptr<CommandChecker> checker;
};

TEST_F(CommandCheckerBacktickTest, BacktickDetected) {
    auto errors = checker->check("echo `ls`");
    bool found = false;
    for (const auto& error : errors) {
        if (error.message.find("backticks") != std::string::npos) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(CommandCheckerBacktickTest, DollarParenNotFlagged) {
    auto errors = checker->check("echo $(ls)");
    bool found = false;
    for (const auto& error : errors) {
        if (error.message.find("backticks") != std::string::npos) {
            found = true;
            break;
        }
    }
    EXPECT_FALSE(found);
}

// ============================================================================
// CommandChecker Infinite Loop Tests
// ============================================================================

class CommandCheckerLoopTest : public ::testing::Test {
protected:
    void SetUp() override { checker = std::make_unique<CommandChecker>(); }

    void TearDown() override { checker.reset(); }

    std::unique_ptr<CommandChecker> checker;
};

TEST_F(CommandCheckerLoopTest, WhileTrueDetected) {
    auto errors = checker->check("while (true); do echo hello; done");
    bool found = false;
    for (const auto& error : errors) {
        if (error.message.find("infinite loop") != std::string::npos) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(CommandCheckerLoopTest, ForeverLoopDetected) {
    auto errors = checker->check("for (;;) { echo hello; }");
    bool found = false;
    for (const auto& error : errors) {
        if (error.message.find("infinite loop") != std::string::npos) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

// ============================================================================
// CommandChecker Privileged Command Tests
// ============================================================================

class CommandCheckerPrivilegedTest : public ::testing::Test {
protected:
    void SetUp() override { checker = std::make_unique<CommandChecker>(); }

    void TearDown() override { checker.reset(); }

    std::unique_ptr<CommandChecker> checker;
};

TEST_F(CommandCheckerPrivilegedTest, SudoDetected) {
    auto errors = checker->check("sudo rm file");
    bool found = false;
    for (const auto& error : errors) {
        if (error.message.find("Privileged command") != std::string::npos &&
            error.message.find("sudo") != std::string::npos) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(CommandCheckerPrivilegedTest, EnablePrivilegedCheck) {
    checker->enablePrivilegedCommandCheck(true);
    auto errors = checker->check("sudo ls");
    bool found = false;
    for (const auto& error : errors) {
        if (error.message.find("Privileged") != std::string::npos) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(CommandCheckerPrivilegedTest, DisablePrivilegedCheck) {
    checker->enablePrivilegedCommandCheck(false);
    auto errors = checker->check("sudo ls");
    bool found = false;
    for (const auto& error : errors) {
        if (error.message.find("Privileged") != std::string::npos) {
            found = true;
            break;
        }
    }
    EXPECT_FALSE(found);
}

// ============================================================================
// CommandChecker Nesting Depth Tests
// ============================================================================

class CommandCheckerNestingTest : public ::testing::Test {
protected:
    void SetUp() override { checker = std::make_unique<CommandChecker>(); }

    void TearDown() override { checker.reset(); }

    std::unique_ptr<CommandChecker> checker;
};

TEST_F(CommandCheckerNestingTest, SetMaxNestingDepth) {
    checker->setMaxNestingDepth(3);

    std::string deeplyNested = "((((((test))))))";
    auto errors = checker->check(deeplyNested);
    bool found = false;
    for (const auto& error : errors) {
        if (error.message.find("nesting depth") != std::string::npos) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(CommandCheckerNestingTest, ShallowNestingOk) {
    checker->setMaxNestingDepth(10);

    std::string shallowNested = "((test))";
    auto errors = checker->check(shallowNested);
    bool found = false;
    for (const auto& error : errors) {
        if (error.message.find("nesting depth") != std::string::npos) {
            found = true;
            break;
        }
    }
    EXPECT_FALSE(found);
}

// ============================================================================
// CommandChecker Forbidden Patterns Tests
// ============================================================================

class CommandCheckerPatternTest : public ::testing::Test {
protected:
    void SetUp() override { checker = std::make_unique<CommandChecker>(); }

    void TearDown() override { checker.reset(); }

    std::unique_ptr<CommandChecker> checker;
};

TEST_F(CommandCheckerPatternTest, SetForbiddenPatterns) {
    std::vector<std::string> patterns = {"secret.*key", "password"};
    checker->setForbiddenPatterns(patterns);

    auto errors = checker->check("echo secret_key=123");
    bool found = false;
    for (const auto& error : errors) {
        if (error.message.find("Forbidden pattern") != std::string::npos) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

// ============================================================================
// CommandChecker Resource Limits Tests
// ============================================================================

class CommandCheckerResourceTest : public ::testing::Test {
protected:
    void SetUp() override { checker = std::make_unique<CommandChecker>(); }

    void TearDown() override { checker.reset(); }

    std::unique_ptr<CommandChecker> checker;
};

TEST_F(CommandCheckerResourceTest, SetResourceLimits) {
    checker->setResourceLimits(512, 50);  // 512MB memory, 50MB file

    auto errors = checker->check("dd if=/dev/zero of=/tmp/test bs=1024MB");
    bool found = false;
    for (const auto& error : errors) {
        if (error.message.find("Memory limit") != std::string::npos) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

// ============================================================================
// CommandChecker Sandbox Tests
// ============================================================================

class CommandCheckerSandboxTest : public ::testing::Test {
protected:
    void SetUp() override { checker = std::make_unique<CommandChecker>(); }

    void TearDown() override { checker.reset(); }

    std::unique_ptr<CommandChecker> checker;
};

TEST_F(CommandCheckerSandboxTest, EnableSandbox) {
    EXPECT_NO_THROW(checker->enableSandbox(true));
}

TEST_F(CommandCheckerSandboxTest, DisableSandbox) {
    EXPECT_NO_THROW(checker->enableSandbox(false));
}

// ============================================================================
// CommandChecker Security Rule Tests
// ============================================================================

class CommandCheckerSecurityRuleTest : public ::testing::Test {
protected:
    void SetUp() override { checker = std::make_unique<CommandChecker>(); }

    void TearDown() override { checker.reset(); }

    std::unique_ptr<CommandChecker> checker;
};

TEST_F(CommandCheckerSecurityRuleTest, AddSecurityRule) {
    checker->addSecurityRule([](const std::string& cmd) {
        return cmd.find("unsafe") == std::string::npos;
    });

    auto errors = checker->check("echo unsafe command");
    bool found = false;
    for (const auto& error : errors) {
        if (error.message.find("security rule") != std::string::npos) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(CommandCheckerSecurityRuleTest, MultipleSecurityRules) {
    checker->addSecurityRule([](const std::string& cmd) {
        return cmd.find("bad1") == std::string::npos;
    });
    checker->addSecurityRule([](const std::string& cmd) {
        return cmd.find("bad2") == std::string::npos;
    });

    auto errors = checker->check("echo bad1 bad2");
    // Should trigger both rules
    int securityErrors = 0;
    for (const auto& error : errors) {
        if (error.message.find("security rule") != std::string::npos) {
            securityErrors++;
        }
    }
    EXPECT_GE(securityErrors, 2);
}

// ============================================================================
// CommandChecker Timeout Tests
// ============================================================================

class CommandCheckerTimeoutTest : public ::testing::Test {
protected:
    void SetUp() override { checker = std::make_unique<CommandChecker>(); }

    void TearDown() override { checker.reset(); }

    std::unique_ptr<CommandChecker> checker;
};

TEST_F(CommandCheckerTimeoutTest, SetTimeoutLimit) {
    EXPECT_NO_THROW(
        checker->setTimeoutLimit(std::chrono::milliseconds(10000)));
}

// ============================================================================
// CommandChecker JSON Tests
// ============================================================================

class CommandCheckerJsonTest : public ::testing::Test {
protected:
    void SetUp() override { checker = std::make_unique<CommandChecker>(); }

    void TearDown() override { checker.reset(); }

    std::unique_ptr<CommandChecker> checker;
};

TEST_F(CommandCheckerJsonTest, ToJson) {
    auto errors = checker->check("rm -rf /");
    auto json = checker->toJson(errors);

    EXPECT_FALSE(json.empty());
    EXPECT_TRUE(json.is_array());
}

TEST_F(CommandCheckerJsonTest, ToJsonEmpty) {
    std::vector<CommandChecker::Error> errors;
    auto json = checker->toJson(errors);

    EXPECT_TRUE(json.is_array());
    EXPECT_TRUE(json.empty());
}

// ============================================================================
// CommandChecker Config Persistence Tests
// ============================================================================

class CommandCheckerConfigTest : public ::testing::Test {
protected:
    void SetUp() override {
        checker = std::make_unique<CommandChecker>();
        testConfigPath =
            std::filesystem::temp_directory_path() / "test_checker_config.json";
    }

    void TearDown() override {
        checker.reset();
        if (std::filesystem::exists(testConfigPath)) {
            std::filesystem::remove(testConfigPath);
        }
    }

    std::unique_ptr<CommandChecker> checker;
    std::filesystem::path testConfigPath;
};

TEST_F(CommandCheckerConfigTest, SaveConfig) {
    checker->setMaxLineLength(120);
    checker->setDangerousCommands({"custom_danger"});

    EXPECT_NO_THROW(checker->saveConfig(testConfigPath.string()));
    EXPECT_TRUE(std::filesystem::exists(testConfigPath));
}

TEST_F(CommandCheckerConfigTest, LoadConfig) {
    // Create a config file
    checker->setMaxLineLength(150);
    checker->saveConfig(testConfigPath.string());

    // Load into new checker
    CommandChecker newChecker;
    EXPECT_NO_THROW(newChecker.loadConfig(testConfigPath.string()));
}

TEST_F(CommandCheckerConfigTest, LoadNonexistentConfig) {
    EXPECT_THROW(checker->loadConfig("/nonexistent/path/config.json"),
                 std::runtime_error);
}

// ============================================================================
// printErrors Function Tests
// ============================================================================

class PrintErrorsTest : public ::testing::Test {
protected:
    void SetUp() override { checker = std::make_unique<CommandChecker>(); }

    void TearDown() override { checker.reset(); }

    std::unique_ptr<CommandChecker> checker;
};

TEST_F(PrintErrorsTest, PrintErrorsWithColor) {
    auto errors = checker->check("rm -rf /");
    EXPECT_NO_THROW(printErrors(errors, "rm -rf /", true));
}

TEST_F(PrintErrorsTest, PrintErrorsWithoutColor) {
    auto errors = checker->check("rm -rf /");
    EXPECT_NO_THROW(printErrors(errors, "rm -rf /", false));
}

TEST_F(PrintErrorsTest, PrintEmptyErrors) {
    std::vector<CommandChecker::Error> errors;
    EXPECT_NO_THROW(printErrors(errors, "echo hello", true));
}

// ============================================================================
// CommandChecker Multiple Errors Tests
// ============================================================================

class CommandCheckerMultipleErrorsTest : public ::testing::Test {
protected:
    void SetUp() override { checker = std::make_unique<CommandChecker>(); }

    void TearDown() override { checker.reset(); }

    std::unique_ptr<CommandChecker> checker;
};

TEST_F(CommandCheckerMultipleErrorsTest, MultipleErrorsDetected) {
    auto errors = checker->check("sudo rm -rf / `ls`");
    // Should detect: sudo (privileged), rm (dangerous), backticks
    EXPECT_GE(errors.size(), 3);
}

TEST_F(CommandCheckerMultipleErrorsTest, ErrorsHaveCorrectSeverity) {
    auto errors = checker->check("rm -rf /");
    for (const auto& error : errors) {
        // Dangerous command should be ERROR severity
        if (error.message.find("Dangerous") != std::string::npos) {
            EXPECT_EQ(error.severity, ErrorSeverity::ERROR);
        }
    }
}

TEST_F(CommandCheckerMultipleErrorsTest, ErrorsHaveLineNumbers) {
    auto errors = checker->check("line1\nrm -rf /\nline3");
    for (const auto& error : errors) {
        if (error.message.find("Dangerous") != std::string::npos) {
            EXPECT_EQ(error.line, 2);  // rm is on line 2
        }
    }
}

}  // namespace lithium::debug::test
