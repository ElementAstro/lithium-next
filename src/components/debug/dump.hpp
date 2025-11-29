#ifndef COREDUMPANALYZER_H
#define COREDUMPANALYZER_H

#include <memory>
#include <string>
#include <vector>

namespace lithium::addon {

/**
 * @brief Analyzes core dump files.
 *
 * The CoreDumpAnalyzer class provides functionality to read and analyze core
 * dump files. It extracts various pieces of information such as memory usage,
 * thread details, stack traces, and generates comprehensive reports based
 * on the analysis.
 */
class CoreDumpAnalyzer {
public:
    /**
     * @brief Constructs a CoreDumpAnalyzer object.
     *
     * Initializes the CoreDumpAnalyzer instance, setting up necessary resources
     * and preparing the analyzer for processing core dump files.
     */
    CoreDumpAnalyzer();

    /**
     * @brief Destructs the CoreDumpAnalyzer object.
     *
     * Cleans up resources used by the CoreDumpAnalyzer instance, ensuring that
     * all allocated memory and handles are properly released.
     */
    ~CoreDumpAnalyzer();

    /**
     * @brief Reads a core dump file.
     *
     * This function reads the specified core dump file and loads its contents
     * into memory, preparing it for subsequent analysis.
     *
     * @param filename The path to the core dump file to be analyzed.
     * @return True if the file was successfully read and loaded, false
     * otherwise.
     */
    bool readFile(const std::string& filename);

    /**
     * @brief Analyzes the core dump file.
     *
     * Performs a comprehensive analysis on the previously read core dump file.
     * This includes parsing ELF headers, extracting memory maps, thread
     * information, and other relevant data necessary for generating detailed
     * reports.
     */
    void analyze();

    /**
     * @brief 获取内存使用详细信息
     *
     * @brief Retrieves detailed memory usage information from the core dump.
     *
     * This function analyzes the memory segments captured in the core dump and
     * aggregates usage statistics, providing insights into how memory was
     * utilized at the time of the crash.
     *
     * @return A string containing formatted memory usage details.
     */
    std::string getDetailedMemoryInfo() const;

    /**
     * @brief 分析堆栈跟踪
     *
     * @brief Analyzes stack traces from the core dump.
     *
     * This function processes the stack frames of each thread captured in the
     * core dump, unwinding the stack to provide a trace of function calls
     * leading up to the point of failure. It assists in identifying the
     * sequence of events that led to the crash.
     *
     * @return A string containing the analyzed stack trace information.
     */
    std::string analyzeStackTrace() const;

    /**
     * @brief 获取详细的线程信息
     *
     * @brief Retrieves detailed thread information from the core dump.
     *
     * This function extracts information about each thread present in the core
     * dump, including thread IDs, register states, CPU usage, and memory usage.
     * It provides a comprehensive view of the state of each thread at the time
     * of the crash.
     *
     * @return A string containing detailed information about each thread.
     */
    std::string getThreadDetails() const;

    /**
     * @brief 生成完整分析报告
     *
     * @brief Generates a comprehensive analysis report from the core dump.
     *
     * This function compiles all the extracted information, including memory
     * usage, thread details, stack traces, ELF header information, and section
     * headers, into a cohesive report. This report is intended to aid
     * developers in diagnosing and debugging issues that led to the crash.
     *
     * @return A string containing the complete analysis report.
     */
    std::string generateReport() const;

    /**
     * @brief 设置分析选项
     *
     * @brief Sets the analysis options for the CoreDumpAnalyzer.
     *
     * Configures which aspects of the core dump should be included in the
     * analysis. Developers can choose to include or exclude memory usage
     * details, thread information, and stack trace analysis based on their
     * specific needs.
     *
     * @param includeMemory If true, memory usage information will be included
     * in the analysis.
     * @param includeThreads If true, thread details will be included in the
     * analysis.
     * @param includeStack If true, stack trace analysis will be performed and
     * included.
     */
    void setAnalysisOptions(bool includeMemory = true,
                            bool includeThreads = true,
                            bool includeStack = true);

    // 新增方法
    [[nodiscard]] auto getProcessInfo() const -> std::string;
    [[nodiscard]] auto getLoadedModules() const -> std::vector<std::string>;
    [[nodiscard]] auto getSignalInfo() const -> std::string;
    [[nodiscard]] auto getResourceUsage() const -> std::string;
    [[nodiscard]] auto getCrashReason() const -> std::string;
    [[nodiscard]] auto getBacktrace(uint64_t threadId) const -> std::string;
    [[nodiscard]] auto getRegistersSnapshot(uint64_t threadId) const
        -> std::string;
    [[nodiscard]] auto getMemoryMap() const -> std::string;
    [[nodiscard]] auto findMemoryLeaks() const -> std::string;
    [[nodiscard]] auto analyzeLockContention() const -> std::string;

    // 导出功能
    void exportToJson(const std::string& filename) const;
    void exportToXml(const std::string& filename) const;
    void generateHtmlReport(const std::string& filename) const;

    // 配置选项
    void setSymbolSearchPaths(const std::vector<std::string>& paths);
    void setAnalysisDepth(int depth);
    void enableMemoryAnalysis(bool enable);
    void enableThreadAnalysis(bool enable);
    void enableResourceAnalysis(bool enable);

private:
    /**
     * @brief Implementation class for CoreDumpAnalyzer.
     *
     * The Impl class encapsulates the internal implementation details of the
     * CoreDumpAnalyzer. It follows the Pimpl idiom to provide a stable
     * interface and hide complex implementation logic from the user.
     */
    class Impl;

    /**
     * @brief Pointer to the implementation of CoreDumpAnalyzer.
     *
     * This unique pointer manages the lifetime of the Impl instance, ensuring
     * proper resource management and encapsulation of implementation details.
     */
    std::unique_ptr<Impl> pImpl_;
};

}  // namespace lithium::addon

#endif  // COREDUMPANALYZER_H
