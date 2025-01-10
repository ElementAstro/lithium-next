#include <gtest/gtest.h>
#include "script/check.hpp"

using namespace lithium;

class ScriptAnalyzerTest : public ::testing::Test {
protected:
    void SetUp() override {
        checker = std::make_unique<ScriptAnalyzer>("config.json");
    }

    void TearDown() override { checker.reset(); }

    std::unique_ptr<ScriptAnalyzer> checker;
};

TEST_F(ScriptAnalyzerTest, ScriptAnalyzer_AnalyzeEmptyScript) {
    ScriptAnalyzer analyzer("test_config.json");
    EXPECT_NO_THROW(analyzer.analyze("", false, ReportFormat::TEXT));
}

TEST_F(ScriptAnalyzerTest, ScriptAnalyzer_DetectPythonScript) {
    ScriptAnalyzer analyzer("test_config.json");
    const std::string python_script =
        "import os\n"
        "def risky_operation():\n"
        "    os.system('rm -rf /')\n";

    std::vector<DangerItem> dangers;
    analyzer.setCallback(
        [&](const DangerItem& item) { dangers.push_back(item); });

    analyzer.analyze(python_script, false, ReportFormat::TEXT);
    ASSERT_FALSE(dangers.empty());
    EXPECT_EQ(dangers[0].category, "Python Script Security Issue");
}

TEST_F(ScriptAnalyzerTest, ScriptAnalyzer_DetectRubyScript) {
    ScriptAnalyzer analyzer("test_config.json");
    const std::string ruby_script =
        "require 'fileutils'\n"
        "def dangerous_method\n"
        "  `rm -rf /`\n"
        "end\n";

    std::vector<DangerItem> dangers;
    analyzer.setCallback(
        [&](const DangerItem& item) { dangers.push_back(item); });

    analyzer.analyze(ruby_script, false, ReportFormat::TEXT);
    ASSERT_FALSE(dangers.empty());
    EXPECT_EQ(dangers[0].category, "Ruby Script Security Issue");
}

TEST_F(ScriptAnalyzerTest, ScriptAnalyzer_DetectShellScript) {
    ScriptAnalyzer analyzer("test_config.json");
    const std::string shell_script =
        "#!/bin/bash\n"
        "rm -rf /\n"
        "kill -9 $$\n";

    std::vector<DangerItem> dangers;
    analyzer.setCallback(
        [&](const DangerItem& item) { dangers.push_back(item); });

    analyzer.analyze(shell_script, false, ReportFormat::TEXT);
    ASSERT_FALSE(dangers.empty());
    EXPECT_EQ(dangers[0].category, "Shell Script Security Issue");
}

TEST_F(ScriptAnalyzerTest, ScriptAnalyzer_JSONOutput) {
    ScriptAnalyzer analyzer("test_config.json");
    const std::string script = "sudo rm -rf /";

    testing::internal::CaptureStdout();
    analyzer.analyze(script, true, ReportFormat::JSON);
    std::string output = testing::internal::GetCapturedStdout();

    EXPECT_TRUE(output.find("\"complexity\":") != std::string::npos);
    EXPECT_TRUE(output.find("\"issues\":") != std::string::npos);
}

TEST_F(ScriptAnalyzerTest, ScriptAnalyzer_XMLOutput) {
    ScriptAnalyzer analyzer("test_config.json");
    const std::string script = "sudo rm -rf /";

    testing::internal::CaptureStdout();
    analyzer.analyze(script, false, ReportFormat::XML);
    std::string output = testing::internal::GetCapturedStdout();

    EXPECT_TRUE(output.find("<Report>") != std::string::npos);
    EXPECT_TRUE(output.find("<Complexity>") != std::string::npos);
    EXPECT_TRUE(output.find("<Issues>") != std::string::npos);
}

TEST_F(ScriptAnalyzerTest, ScriptAnalyzer_ComplexityCalculation) {
    ScriptAnalyzer analyzer("test_config.json");
    const std::string complex_script =
        "if true; then\n"
        "  while read line; do\n"
        "    case $line in\n"
        "      *) echo $line;;\n"
        "    esac\n"
        "  done\n"
        "fi\n";

    std::vector<DangerItem> dangers;
    analyzer.setCallback(
        [&](const DangerItem& item) { dangers.push_back(item); });

    testing::internal::CaptureStdout();
    analyzer.analyze(complex_script, false, ReportFormat::TEXT);
    std::string output = testing::internal::GetCapturedStdout();

    EXPECT_TRUE(output.find("Code Complexity: 3") != std::string::npos);
}

TEST_F(ScriptAnalyzerTest, ScriptAnalyzer_ConcurrentAnalysis) {
    ScriptAnalyzer analyzer("test_config.json");
    const std::string script =
        "curl http://example.com\n"
        "wget http://example.com\n"
        "sudo rm -rf /\n"
        "export SECRET_KEY='123'\n";

    std::vector<DangerItem> dangers;
    analyzer.setCallback(
        [&](const DangerItem& item) { dangers.push_back(item); });

    analyzer.analyze(script, false, ReportFormat::TEXT);

    // Should detect external commands, dangerous commands, and env vars
    EXPECT_GE(dangers.size(), 3);
}

TEST_F(ScriptAnalyzerTest, ScriptAnalyzer_ErrorHandling) {
    ScriptAnalyzer analyzer("nonexistent_config.json");
    const std::string script = "echo 'test'";

    EXPECT_THROW(analyzer.analyze(script, false, ReportFormat::TEXT),
                 InvalidFormatException);
}

TEST_F(ScriptAnalyzerTest, ScriptAnalyzer_StatisticsTracking) {
    ScriptAnalyzer analyzer("test_config.json");
    const std::string script = "echo 'test'";

    size_t initial_count = analyzer.getTotalAnalyzed();
    analyzer.analyze(script, false, ReportFormat::TEXT);

    EXPECT_EQ(analyzer.getTotalAnalyzed(), initial_count + 1);
    EXPECT_GT(analyzer.getAverageAnalysisTime(), 0.0);
}