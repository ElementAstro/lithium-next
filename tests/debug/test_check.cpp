#ifndef LITHIUM_DEBUG_TEST_CHECK_HPP
#define LITHIUM_DEBUG_TEST_CHECK_HPP

#include <gtest/gtest.h>
#include "debug/check.hpp"

using namespace lithium::debug;

class CommandCheckerTest : public ::testing::Test {
protected:
    void SetUp() override { checker = std::make_unique<CommandChecker>(); }

    void TearDown() override { checker.reset(); }

    std::unique_ptr<CommandChecker> checker;
};

TEST_F(CommandCheckerTest, CheckEmptyCommand) {
    auto errors = checker->check("");
    ASSERT_EQ(errors.size(), 1);
    EXPECT_EQ(errors[0].message, "Empty input string");
    EXPECT_EQ(errors[0].severity, CommandChecker::ErrorSeverity::ERROR);
}

TEST_F(CommandCheckerTest, CheckDangerousCommand) {
    auto errors = checker->check("rm -rf /");
    ASSERT_EQ(errors.size(), 1);
    EXPECT_EQ(errors[0].message, "Dangerous command detected: rm");
    EXPECT_EQ(errors[0].severity, CommandChecker::ErrorSeverity::ERROR);
}

TEST_F(CommandCheckerTest, CheckLongLine) {
    std::string longCommand(100, 'x');
    auto errors = checker->check(longCommand);
    ASSERT_EQ(errors.size(), 1);
    EXPECT_EQ(errors[0].message, "Line exceeds maximum length");
    EXPECT_EQ(errors[0].severity, CommandChecker::ErrorSeverity::WARNING);
}

TEST_F(CommandCheckerTest, CheckUnmatchedQuotes) {
    auto errors = checker->check("echo \"hello");
    ASSERT_EQ(errors.size(), 1);
    EXPECT_EQ(errors[0].message, "Unmatched double quotes detected");
    EXPECT_EQ(errors[0].severity, CommandChecker::ErrorSeverity::ERROR);
}

TEST_F(CommandCheckerTest, CheckBacktickUsage) {
    auto errors = checker->check("echo `ls`");
    ASSERT_EQ(errors.size(), 1);
    EXPECT_EQ(errors[0].message,
              "Use of backticks detected, consider using $() instead");
    EXPECT_EQ(errors[0].severity, CommandChecker::ErrorSeverity::WARNING);
}

TEST_F(CommandCheckerTest, CheckUnusedVariable) {
    auto errors = checker->check("VAR=10\necho hello");
    ASSERT_EQ(errors.size(), 1);
    EXPECT_EQ(errors[0].message, "Unused variable detected: VAR");
    EXPECT_EQ(errors[0].severity, CommandChecker::ErrorSeverity::WARNING);
}

TEST_F(CommandCheckerTest, CheckInfiniteLoop) {
    auto errors = checker->check("while (true); do echo hello; done");
    ASSERT_EQ(errors.size(), 1);
    EXPECT_EQ(errors[0].message, "Potential infinite loop detected");
    EXPECT_EQ(errors[0].severity, CommandChecker::ErrorSeverity::WARNING);
}

TEST_F(CommandCheckerTest, CheckPrivilegedCommand) {
    auto errors = checker->check("sudo rm file");
    ASSERT_EQ(errors.size(), 2);  // Both sudo and rm should be detected
    EXPECT_EQ(errors[0].message, "Privileged command detected: sudo");
    EXPECT_EQ(errors[0].severity, CommandChecker::ErrorSeverity::WARNING);
}

TEST_F(CommandCheckerTest, CheckResourceLimits) {
    auto errors = checker->check("dd if=/dev/zero of=/tmp/test bs=2048MB");
    ASSERT_GE(errors.size(), 1);
    bool hasMemoryError = false;
    for (const auto& error : errors) {
        if (error.message.find("Memory limit exceeded") != std::string::npos) {
            hasMemoryError = true;
            EXPECT_EQ(error.severity, CommandChecker::ErrorSeverity::ERROR);
            break;
        }
    }
    EXPECT_TRUE(hasMemoryError);
}

TEST_F(CommandCheckerTest, ConfigurationTest) {
    std::vector<std::string> customDangerousCommands = {"danger1", "danger2"};
    checker->setDangerousCommands(customDangerousCommands);

    auto errors = checker->check("danger1 something");
    ASSERT_EQ(errors.size(), 1);
    EXPECT_EQ(errors[0].message, "Dangerous command detected: danger1");
}

TEST_F(CommandCheckerTest, MultipleErrors) {
    auto errors = checker->check("sudo rm -rf / `ls`");
    ASSERT_GE(errors.size(), 3);  // Should detect sudo, rm, and backticks
}

TEST_F(CommandCheckerTest, NoErrors) {
    auto errors = checker->check("echo hello");
    EXPECT_EQ(errors.size(), 0);
}

#endif  // LITHIUM_DEBUG_TEST_CHECK_HPP
