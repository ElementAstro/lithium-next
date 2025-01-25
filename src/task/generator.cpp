/**
 * @file generator.cpp
 * @brief Task Generator
 *
 * This file contains the definition and implementation of a task generator.
 *
 * @date 2023-07-21
 * @modified 2024-04-27
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2024 Max Qian
 */

#include "generator.hpp"

#include <algorithm>
#include <cctype>
#include <mutex>
#include <shared_mutex>
#include <sstream>
#include <string>
#include <vector>

#if ENABLE_FASTHASH
#include "emhash/hash_table8.hpp"
#else
#include <unordered_map>
#endif

#ifdef LITHIUM_USE_BOOST_REGEX
#include <boost/regex.hpp>
#else
#include <regex>
#endif

#include "atom/log/loguru.hpp"
#include "atom/type/json.hpp"
#include "atom/utils/string.hpp"

using json = nlohmann::json;
using namespace std::literals;

namespace lithium {

#ifdef LITHIUM_USE_BOOST_REGEX
using Regex = boost::regex;
using Match = boost::smatch;
#else
using Regex = std::regex;
using Match = std::smatch;
#endif

class TaskGenerator::Impl {
public:
    Impl();
    void addMacro(const std::string& name, MacroValue value);
    void removeMacro(const std::string& name);
    auto listMacros() const -> std::vector<std::string>;
    void processJson(json& json_obj);
    void processJsonWithJsonMacros(json& json_obj);

    static constexpr size_t DEFAULT_MAX_CACHE_SIZE = 1000;

    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, MacroValue> macros_;

    // Cache for macro replacements
    mutable std::unordered_map<std::string, std::string> macro_cache_;
    mutable std::shared_mutex cache_mutex_;
    size_t max_cache_size_ = DEFAULT_MAX_CACHE_SIZE;
    Statistics stats_;

    // Precompiled regex patterns
    static const Regex MACRO_PATTERN;
    static const Regex ARG_PATTERN;

    // Helper methods
    auto replaceMacros(const std::string& input) -> std::string;
    auto evaluateMacro(const std::string& name,
                       const std::vector<std::string>& args) const
        -> std::string;
    void preprocessJsonMacros(json& json_obj);
    void trimCache();
};

const Regex TaskGenerator::Impl::MACRO_PATTERN(
    R"(\$\{([^\{\}]+(?:\([^\{\}]*\))*)\})", Regex::optimize);
const Regex TaskGenerator::Impl::ARG_PATTERN(R"(([^,]+))", Regex::optimize);

TaskGenerator::Impl::Impl() {
    // Initialize default macros
    addMacro("uppercase",
             [](const std::vector<std::string>& args) -> std::string {
                 if (args.empty()) {
                     throw TaskGeneratorException(
                         TaskGeneratorException::ErrorCode::INVALID_MACRO_ARGS,
                         "uppercase macro requires at least 1 argument");
                 }
                 std::string result = args[0];
                 std::transform(result.begin(), result.end(), result.begin(),
                                ::toupper);
                 return result;
             });
    addMacro("concat", [](const std::vector<std::string>& args) -> std::string {
        if (args.empty()) {
            return "";
        }

        std::ostringstream oss;
        oss << args[0];
        for (size_t i = 1; i < args.size(); ++i) {
            if (!args[i].empty()) {
                if ((std::ispunct(args[i][0]) != 0) && args[i][0] != '(' &&
                    args[i][0] != '[') {
                    oss << args[i];
                } else {
                    oss << " " << args[i];
                }
            }
        }
        return oss.str();
    });

    addMacro("if", [](const std::vector<std::string>& args) -> std::string {
        if (args.size() < 3) {
            throw TaskGeneratorException(
                TaskGeneratorException::ErrorCode::INVALID_MACRO_ARGS,
                "if macro requires 3 arguments");
        }
        return args[0] == "true" ? args[1] : args[2];
    });
    addMacro("length", [](const std::vector<std::string>& args) -> std::string {
        if (args.size() != 1) {
            throw TaskGeneratorException(
                TaskGeneratorException::ErrorCode::INVALID_MACRO_ARGS,
                "length macro requires 1 argument");
        }
        return std::to_string(args[0].length());
    });
    addMacro("equals", [](const std::vector<std::string>& args) -> std::string {
        if (args.size() != 2) {
            throw TaskGeneratorException(
                TaskGeneratorException::ErrorCode::INVALID_MACRO_ARGS,
                "equals macro requires 2 arguments");
        }
        return args[0] == args[1] ? "true" : "false";
    });
    addMacro("tolower",
             [](const std::vector<std::string>& args) -> std::string {
                 if (args.empty()) {
                     throw TaskGeneratorException(
                         TaskGeneratorException::ErrorCode::INVALID_MACRO_ARGS,
                         "tolower macro requires at least 1 argument");
                 }
                 std::string result = args[0];
                 std::transform(result.begin(), result.end(), result.begin(),
                                ::tolower);
                 return result;
             });
    addMacro("repeat", [](const std::vector<std::string>& args) -> std::string {
        if (args.size() != 2) {
            throw TaskGeneratorException(
                TaskGeneratorException::ErrorCode::INVALID_MACRO_ARGS,
                "repeat macro requires 2 arguments");
        }
        std::string result;
        try {
            int times = std::stoi(args[1]);
            if (times < 0) {
                throw std::invalid_argument("Negative repeat count");
            }
            result.reserve(args[0].size() * times);
            for (int i = 0; i < times; ++i) {
                result += args[0];
            }
        } catch (const std::exception& e) {
            throw TaskGeneratorException(
                TaskGeneratorException::ErrorCode::INVALID_MACRO_ARGS,
                std::string("Invalid repeat count: ") + e.what());
        }
        return result;
    });
}

void TaskGenerator::Impl::addMacro(const std::string& name, MacroValue value) {
    if (name.empty()) {
        throw TaskGeneratorException(
            TaskGeneratorException::ErrorCode::INVALID_MACRO_ARGS,
            "Macro name cannot be empty");
    }
    LOG_F(INFO, "Adding macro: {}", name);

    std::unique_lock lock(mutex_);
    macros_[name] = std::move(value);

    std::unique_lock cacheLock(cache_mutex_);
    macro_cache_.clear();
    LOG_F(INFO, "Cache cleared after adding macro: {}", name);
}

void TaskGenerator::Impl::removeMacro(const std::string& name) {
    std::unique_lock lock(mutex_);
    auto it = macros_.find(name);
    if (it != macros_.end()) {
        macros_.erase(it);
        // Invalidate cache as macros have changed
        std::unique_lock cacheLock(cache_mutex_);
        macro_cache_.clear();
    } else {
        throw TaskGeneratorException(
            TaskGeneratorException::ErrorCode::INVALID_MACRO_ARGS,
            "Attempted to remove undefined macro: " + name);
    }
}

auto TaskGenerator::Impl::listMacros() const -> std::vector<std::string> {
    std::shared_lock lock(mutex_);
    std::vector<std::string> keys;
    keys.reserve(macros_.size());
    for (const auto& [key, _] : macros_) {
        keys.emplace_back(key);
    }
    return keys;
}

void TaskGenerator::Impl::processJson(json& json_obj) {
    try {
        for (const auto& [key, value] : json_obj.items()) {
            if (value.is_string()) {
                value = replaceMacros(value.get<std::string>());
            } else if (value.is_object() || value.is_array()) {
                processJson(value);
            }
        }
    } catch (const TaskGeneratorException& e) {
        LOG_F(ERROR, "Error processing JSON: {}", e.what());
        throw;
    }
}

void TaskGenerator::Impl::processJsonWithJsonMacros(json& json_obj) {
    try {
        preprocessJsonMacros(json_obj);  // Preprocess macros
        processJson(json_obj);           // Replace macros in JSON
    } catch (const TaskGeneratorException& e) {
        LOG_F(ERROR, "Error processing JSON with macros: {}", e.what());
        throw;
    }
}

auto TaskGenerator::Impl::replaceMacros(const std::string& input)
    -> std::string {
    auto start_time = std::chrono::high_resolution_clock::now();
    std::string result = input;
    Match match;

    try {
        while (std::regex_search(result, match, MACRO_PATTERN)) {
            std::string fullMatch = match[0];
            std::string macroContent = match[1].str();

            {
                std::shared_lock cacheLock(cache_mutex_);
                auto cacheIt = macro_cache_.find(macroContent);
                if (cacheIt != macro_cache_.end()) {
                    ++stats_.cache_hits;
                    result.replace(match.position(0), match.length(0),
                                   cacheIt->second);
                    continue;
                }
            }

            ++stats_.cache_misses;
            std::string replacement = [&]() -> std::string {
                try {
                    std::string macroName;
                    std::vector<std::string> args;

                    auto pos = macroContent.find('(');
                    if (pos == std::string::npos) {
                        macroName = macroContent;
                    } else {
                        if (macroContent.back() != ')') {
                            throw TaskGeneratorException(
                                TaskGeneratorException::ErrorCode::
                                    INVALID_MACRO_ARGS,
                                "Malformed macro definition: " + macroContent);
                        }
                        macroName = macroContent.substr(0, pos);
                        std::string argsStr = macroContent.substr(
                            pos + 1, macroContent.length() - pos - 2);

                        std::sregex_token_iterator iter(
                            argsStr.begin(), argsStr.end(), ARG_PATTERN);
                        std::sregex_token_iterator end;
                        for (; iter != end; ++iter) {
                            args.emplace_back(atom::utils::trim(iter->str()));
                        }
                    }

                    return evaluateMacro(macroName, args);
                } catch (const TaskGeneratorException& e) {
                    LOG_F(ERROR, "Error evaluating macro '{}': {}",
                          macroContent, e.what());
                    throw;
                }
            }();

            {
                std::unique_lock cacheLock(cache_mutex_);
                macro_cache_[macroContent] = replacement;
                trimCache();
            }

            result.replace(match.position(0), match.length(0), replacement);
        }
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Error in macro replacement: {}", e.what());
        throw TaskGeneratorException(
            TaskGeneratorException::ErrorCode::MACRO_EVALUATION_ERROR,
            std::string("Macro replacement failed: ") + e.what());
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
                        end_time - start_time)
                        .count();

    ++stats_.macro_evaluations;
    stats_.average_evaluation_time =
        (stats_.average_evaluation_time * (stats_.macro_evaluations - 1) +
         duration) /
        stats_.macro_evaluations;

    return result;
}

auto TaskGenerator::Impl::evaluateMacro(
    const std::string& name,
    const std::vector<std::string>& args) const -> std::string {
    std::shared_lock lock(mutex_);
    auto it = macros_.find(name);
    if (it == macros_.end()) {
        throw TaskGeneratorException(
            TaskGeneratorException::ErrorCode::INVALID_MACRO_ARGS,
            "Undefined macro: " + name);
    }

    if (std::holds_alternative<std::string>(it->second)) {
        return std::get<std::string>(it->second);
    }
    if (std::holds_alternative<
            std::function<std::string(const std::vector<std::string>&)>>(
            it->second)) {
        try {
            return std::get<
                std::function<std::string(const std::vector<std::string>&)>>(
                it->second)(args);
        } catch (const std::exception& e) {
            throw TaskGeneratorException(
                TaskGeneratorException::ErrorCode::MACRO_EVALUATION_ERROR,
                "Error evaluating macro '" + name + "': " + e.what());
        }
    } else {
        throw TaskGeneratorException(
            TaskGeneratorException::ErrorCode::INVALID_MACRO_ARGS,
            "Invalid macro type for: " + name);
    }
}

void TaskGenerator::Impl::preprocessJsonMacros(json& json_obj) {
    try {
        for (const auto& [key, value] : json_obj.items()) {
            if (value.is_string()) {
                std::string strValue = value.get<std::string>();
                Match match;
                if (std::regex_match(strValue, match, MACRO_PATTERN)) {
                    std::string macroContent = match[1].str();
                    std::string macroName;
                    std::vector<std::string> args;

                    auto pos = macroContent.find('(');
                    if (pos == std::string::npos) {
                        macroName = macroContent;
                    } else {
                        if (macroContent.back() != ')') {
                            throw TaskGeneratorException(
                                TaskGeneratorException::ErrorCode::
                                    INVALID_MACRO_ARGS,
                                "Malformed macro definition: " + macroContent);
                        }
                        macroName = macroContent.substr(0, pos);
                        std::string argsStr = macroContent.substr(
                            pos + 1, macroContent.length() - pos - 2);

                        std::sregex_token_iterator iter(
                            argsStr.begin(), argsStr.end(), ARG_PATTERN);
                        std::sregex_token_iterator end;
                        for (; iter != end; ++iter) {
                            args.emplace_back(atom::utils::trim(iter->str()));
                        }
                    }

                    // Define macro if not already present
                    {
                        std::unique_lock lock(mutex_);
                        if (macros_.find(key) == macros_.end()) {
                            if (args.empty()) {
                                macros_[key] = macroContent;
                            } else {
                                // For simplicity, store as a concatenated
                                // string
                                macros_[key] = "macro_defined_in_json";
                            }
                        }
                    }

                    LOG_F(INFO, "Preprocessed macro: {} -> {}", key,
                          macroContent);
                }
            } else if (value.is_object() || value.is_array()) {
                preprocessJsonMacros(value);
            }
        }
    } catch (const TaskGeneratorException& e) {
        LOG_F(ERROR, "Error preprocessing JSON macros: {}", e.what());
        throw;
    }
}

void TaskGenerator::Impl::trimCache() {
    if (macro_cache_.size() > max_cache_size_) {
        size_t to_remove = macro_cache_.size() - max_cache_size_;
        for (auto it = macro_cache_.begin(); to_remove > 0; --to_remove) {
            it = macro_cache_.erase(it);
        }
    }
}

TaskGenerator::TaskGenerator() : impl_(std::make_unique<Impl>()) {}

TaskGenerator::~TaskGenerator() = default;

auto TaskGenerator::createShared() -> std::shared_ptr<TaskGenerator> {
    return std::make_shared<TaskGenerator>();
}

void TaskGenerator::addMacro(const std::string& name, MacroValue value) {
    impl_->addMacro(name, std::move(value));
}

void TaskGenerator::removeMacro(const std::string& name) {
    impl_->removeMacro(name);
}

auto TaskGenerator::listMacros() const -> std::vector<std::string> {
    return impl_->listMacros();
}

void TaskGenerator::processJson(json& json_obj) const {
    impl_->processJson(json_obj);
}

void TaskGenerator::processJsonWithJsonMacros(json& json_obj) {
    impl_->processJsonWithJsonMacros(json_obj);
}

void TaskGenerator::clearMacroCache() {
    std::unique_lock cacheLock(impl_->cache_mutex_);
    impl_->macro_cache_.clear();
    LOG_F(INFO, "Macro cache cleared");
}

bool TaskGenerator::hasMacro(const std::string& name) const {
    std::shared_lock lock(impl_->mutex_);
    return impl_->macros_.find(name) != impl_->macros_.end();
}

size_t TaskGenerator::getCacheSize() const {
    std::shared_lock cacheLock(impl_->cache_mutex_);
    return impl_->macro_cache_.size();
}

void TaskGenerator::setMaxCacheSize(size_t size) {
    std::unique_lock lock(impl_->mutex_);
    impl_->max_cache_size_ = size;
    impl_->trimCache();
}

TaskGenerator::Statistics TaskGenerator::getStatistics() const {
    return impl_->stats_;
}

void TaskGenerator::resetStatistics() {
    impl_->stats_ = Statistics{};
    LOG_F(INFO, "Statistics reset");
}

}  // namespace lithium
