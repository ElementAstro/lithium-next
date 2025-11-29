#ifndef LITHIUM_DEBUG_CHECK_HPP
#define LITHIUM_DEBUG_CHECK_HPP

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <string>
#include <vector>

#include "atom/macro.hpp"
#include "atom/type/concurrent_vector.hpp"
#include "atom/type/json_fwd.hpp"
using json = nlohmann::json;

namespace lithium::debug {
/**
 * @brief Enum representing the severity of an error.
 */
enum class ErrorSeverity {
    WARNING,  ///< Warning level error
    ERROR,    ///< Error level error
    CRITICAL  ///< Critical level error
};

template <typename T>
concept CheckRule = requires(T t) {
    { t.check(std::string{}) } -> std::convertible_to<bool>;
    { t.severity() } -> std::convertible_to<ErrorSeverity>;
    { t.message() } -> std::convertible_to<std::string>;
};

/**
 * @brief Class for checking commands against a set of rules.
 */
class CommandChecker {
public:
    /**
     * @brief Struct representing an error found during command checking.
     */
    struct Error {
        std::string message;     ///< The error message
        size_t line;             ///< The line number where the error occurred
        size_t column;           ///< The column number where the error occurred
        ErrorSeverity severity;  ///< The severity of the error
    } ATOM_ALIGNAS(64);

    /**
     * @brief Struct representing a rule for checking commands.
     */
    struct CheckRule {
        std::string name;  ///< The name of the rule
        std::function<std::optional<Error>(const std::string&, size_t)>
            check;  ///< The function to check the rule
    } ATOM_ALIGNAS(64);

    /**
     * @brief Constructs a new CommandChecker object.
     */
    CommandChecker();

    /**
     * @brief Destroys the CommandChecker object.
     */
    ~CommandChecker();

    /**
     * @brief Adds a new rule to the CommandChecker.
     *
     * @param name The name of the rule.
     * @param check The function to check the rule.
     */
    void addRule(
        const std::string& name,
        std::function<std::optional<Error>(const std::string&, size_t)> check);

    /**
     * @brief Adds a typed rule that meets the CheckRule concept
     *
     * @tparam T A type satisfying the CheckRule concept
     * @param name The name of the rule
     * @param rule The rule object
     */
    template <typename T>
    void addTypedRule(const std::string& name, T&& rule) {
        std::unique_lock lock(ruleMutex_);

        auto wrapper = [r = std::forward<T>(rule)](
                           const std::string& line,
                           size_t lineNum) -> std::optional<Error> {
            if (!r.check(line)) {
                return Error{r.message(), lineNum,
                             0,  // Column unknown
                             r.severity()};
            }
            return std::nullopt;
        };

        auto newRule = std::make_unique<CheckRule>();
        newRule->name = name;
        newRule->check = wrapper;
        rules_.push_back(std::move(newRule));
    }

    /**
     * @brief Removes a rule by name.
     * @param name The name of the rule to remove.
     * @return True if the rule was removed, false if not found.
     */
    bool removeRule(const std::string& name);

    /**
     * @brief Lists all rule names currently registered.
     * @return A vector of rule names.
     */
    std::vector<std::string> listRules() const;

    /**
     * @brief Sets the list of dangerous commands.
     *
     * @param commands The list of dangerous commands.
     */
    void setDangerousCommands(const std::vector<std::string>& commands);

    /**
     * @brief Sets the maximum allowed line length for commands.
     *
     * @param length The maximum line length.
     */
    void setMaxLineLength(size_t length);

    /**
     * @brief Checks a command against the set rules.
     *
     * @param command The command to check.
     * @return A vector of errors found during the check.
     */
    ATOM_NODISCARD auto check(std::string_view command) const
        -> std::vector<Error>;

    /**
     * @brief Converts a list of errors to JSON format.
     *
     * @param errors The list of errors to convert.
     * @return The JSON representation of the errors.
     */
    ATOM_NODISCARD auto toJson(const std::vector<Error>& errors) const -> json;

    /**
     * @brief 从JSON文件加载配置
     * @param configPath JSON配置文件路径
     */
    void loadConfig(const std::string& configPath);

    /**
     * @brief 保存当前配置到JSON文件
     * @param configPath JSON配置文件路径
     */
    void saveConfig(const std::string& configPath) const;

    /**
     * @brief 设置允许的最大嵌套深度
     */
    void setMaxNestingDepth(size_t depth);

    /**
     * @brief 设置禁止使用的命令模式
     */
    void setForbiddenPatterns(const std::vector<std::string>& patterns);

    /**
     * @brief 设置是否启用特权命令检查
     */
    void enablePrivilegedCommandCheck(bool enable);

    /**
     * @brief 设置资源限制检查阈值
     */
    void setResourceLimits(size_t maxMemoryMB, size_t maxFileSize);

    /**
     * @brief 设置命令沙箱模式
     * @param enable Whether to enable sandbox mode
     */
    void enableSandbox(bool enable);

    /**
     * @brief 添加自定义安全检查规则
     * @param rule A function that returns true if command is safe, false
     * otherwise
     */
    void addSecurityRule(std::function<bool(const std::string&)> rule);

    /**
     * @brief 设置命令超时限制
     * @param timeout Maximum allowed time for command execution
     */
    void setTimeoutLimit(std::chrono::milliseconds timeout);

private:
    /**
     * @brief Implementation class for CommandChecker.
     */
    class CommandCheckerImpl;
    std::unique_ptr<CommandCheckerImpl>
        impl_;  ///< Pointer to the implementation

    // 线程安全的规则容器
    mutable std::shared_mutex ruleMutex_;
    atom::type::concurrent_vector<std::unique_ptr<CheckRule>> rules_;
};

/**
 * @brief Prints a list of errors to the console.
 *
 * @param errors The list of errors to print.
 * @param command The command that was checked.
 * @param useColor Whether to use color in the output.
 */
void printErrors(const std::vector<CommandChecker::Error>& errors,
                 std::string_view command, bool useColor);

}  // namespace lithium::debug

#endif  // LITHIUM_DEBUG_CHECK_HPP
