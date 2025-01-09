#ifndef LITHIUM_TARGET_READER_CSV
#define LITHIUM_TARGET_READER_CSV

#include <istream>
#include <memory>
#include <ostream>
#include <string>
#include <unordered_map>
#include <vector>
#include <functional>

namespace lithium::target {

// 扩展编码支持
enum class Encoding { UTF8, UTF16, UTF16LE, UTF16BE, GBK, ASCII };

// 添加错误类型定义
enum class CSVError {
    NONE,
    INVALID_FORMAT,
    ENCODING_ERROR,
    IO_ERROR,
    FIELD_MISMATCH
};

// 进度回调函数类型
using ProgressCallback = std::function<void(size_t current, size_t total)>;

// 支持的字符编码
// enum class Encoding { UTF8, UTF16 };

// 引用模式
enum class Quoting { MINIMAL, ALL, NONNUMERIC, STRINGS, NOTNULL, NONE };

// CSV 方言配置
struct Dialect {
    char delimiter = ',';
    char quotechar = '"';
    bool doublequote = true;
    bool skip_initial_space = false;
    std::string lineterminator = "\n";
    Quoting quoting = Quoting::MINIMAL;

    // 添加新配置选项
    size_t buffer_size = 8192;
    bool validate_fields = true;
    bool ignore_errors = false;

    Dialect() = default;
    Dialect(char delim, char quote, bool dquote, bool skipspace,
            std::string lineterm, Quoting quote_mode);
};

// 字典读取器
class DictReader {
public:
    DictReader(std::istream& input, const std::vector<std::string>& fieldnames,
               Dialect dialect = Dialect(), Encoding encoding = Encoding::UTF8);

    ~DictReader();

    bool next(std::unordered_map<std::string, std::string>& row);

    // 新增方法
    void setProgressCallback(ProgressCallback callback);
    [[nodiscard]] auto getLastError() const -> CSVError;
    void enableFieldValidation(bool enable);
    [[nodiscard]] auto getLineNumber() const -> size_t;
    void reset();
    
    // 批量读取方法
    auto readRows(size_t count) -> std::vector<std::unordered_map<std::string, std::string>>;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

// 字典写入器
class DictWriter {
public:
    DictWriter(std::ostream& output, const std::vector<std::string>& fieldnames,
               Dialect dialect = Dialect(), bool quote_all = false,
               Encoding encoding = Encoding::UTF8);

    ~DictWriter();

    void writeRow(const std::unordered_map<std::string, std::string>& row);

    // 新增方法
    void setProgressCallback(ProgressCallback callback);
    void enableChecksum(bool enable);
    void flush();
    [[nodiscard]] auto getWrittenRows() const -> size_t;
    
    // 批量写入方法
    void writeRows(const std::vector<std::unordered_map<std::string, std::string>>& rows);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};
}  // namespace lithium::target

#endif  // LITHIUM_TARGET_READER_CSV
