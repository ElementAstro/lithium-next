// generator.hpp
/**
 * @file generator.hpp
 * @brief Task Generator
 *
 * This file contains the definition and implementation of a task generator.
 *
 * @date 2023-07-21
 * @modified 2024-04-27
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2024 Max Qian
 */

#ifndef LITHIUM_TASK_GENERATOR_HPP
#define LITHIUM_TASK_GENERATOR_HPP

#include <functional>
#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "atom/type/json_fwd.hpp"
using json = nlohmann::json;

namespace lithium {

// 扩展异常类型
class TaskGeneratorException : public std::exception {
public:
    enum class ErrorCode {
        UNDEFINED_MACRO,
        INVALID_MACRO_ARGS,
        MACRO_EVALUATION_ERROR,
        JSON_PROCESSING_ERROR,
        INVALID_MACRO_TYPE,
        CACHE_ERROR
    };

    TaskGeneratorException(ErrorCode code, const std::string& message)
        : code_(code), msg_(message) {}
    
    const char* what() const noexcept override { return msg_.c_str(); }
    ErrorCode code() const noexcept { return code_; }

private:
    ErrorCode code_;
    std::string msg_;
};

using MacroValue =
    std::variant<std::string,
                 std::function<std::string(const std::vector<std::string>&)>>;

class TaskGenerator {
public:
    TaskGenerator();
    ~TaskGenerator();

    static auto createShared() -> std::shared_ptr<TaskGenerator>;

    void addMacro(const std::string& name, MacroValue value);
    void removeMacro(const std::string& name);
    std::vector<std::string> listMacros() const;
    void processJson(json& j) const;
    void processJsonWithJsonMacros(json& j);

    // 新增功能
    void clearMacroCache();
    bool hasMacro(const std::string& name) const;
    size_t getCacheSize() const;
    void setMaxCacheSize(size_t size);
    
    // 性能统计
    struct Statistics {
        size_t cache_hits = 0;
        size_t cache_misses = 0;
        size_t macro_evaluations = 0;
        double average_evaluation_time = 0.0;
    };
    
    Statistics getStatistics() const;
    void resetStatistics();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;  // Pimpl for encapsulation
};

}  // namespace lithium

#endif  // LITHIUM_TASK_GENERATOR_HPP
