/**
 * @file generator.cpp
 * @brief Task Generator implementation
 *
 * @date 2023-07-21
 * @modified 2024-04-27
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2024 Max Qian
 */

#include "generator.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <vector>

#if ENABLE_FASTHASH
#include "emhash/hash_table8.hpp"
using StringMap = emhash8::HashMap<std::string, std::string>;
using MacroMap = emhash8::HashMap<std::string, MacroValue>;
#else
#include <unordered_map>
using StringMap = std::unordered_map<std::string, std::string>;
using MacroMap = std::unordered_map<std::string, lithium::MacroValue>;
#endif

#ifdef LITHIUM_USE_BOOST_REGEX
#include <boost/regex.hpp>
#else
#include <regex>
#endif

#include <spdlog/spdlog.h>
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
    void clearMacroCache();
    bool hasMacro(const std::string& name) const;
    size_t getCacheSize() const;
    void setMaxCacheSize(size_t size);
    void resetStatistics();

    // Script generation methods
    void setScriptConfig(const TaskGenerator::ScriptConfig& config);
    TaskGenerator::ScriptConfig getScriptConfig() const;
    void registerScriptTemplate(const std::string& templateName, const TaskGenerator::ScriptTemplate& templateInfo);
    void unregisterScriptTemplate(const std::string& templateName);
    TaskGenerator::ScriptGenerationResult generateScript(const std::string& templateName, const json& parameters);
    TaskGenerator::ScriptGenerationResult generateSequenceScript(const json& sequenceConfig);
    TaskGenerator::ScriptGenerationResult parseScript(const std::string& script, const std::string& format);
    std::vector<std::string> getAvailableTemplates() const;
    std::optional<TaskGenerator::ScriptTemplate> getTemplateInfo(const std::string& templateName) const;
    TaskGenerator::ScriptGenerationResult generateCustomTaskScript(const std::string& taskType, const json& taskConfig);
    TaskGenerator::ScriptGenerationResult optimizeScript(const std::string& script);
    bool validateScript(const std::string& script, const std::string& templateName);
    size_t loadTemplatesFromDirectory(const std::string& templateDir);
    bool saveTemplatesToDirectory(const std::string& templateDir) const;
    TaskGenerator::ScriptGenerationResult convertScriptFormat(const std::string& script, 
                                                             const std::string& fromFormat,
                                                             const std::string& toFormat);

    static constexpr size_t DEFAULT_MAX_CACHE_SIZE = 1000;

    mutable std::shared_mutex mutex_;
    MacroMap macros_;

    // Cache for macro replacements
    mutable StringMap macro_cache_;
    mutable std::shared_mutex cache_mutex_;
    size_t max_cache_size_ = DEFAULT_MAX_CACHE_SIZE;
    Statistics stats_;

    // Script generation members
    TaskGenerator::ScriptConfig scriptConfig_;
    std::unordered_map<std::string, TaskGenerator::ScriptTemplate> scriptTemplates_;
    mutable std::shared_mutex scriptMutex_;

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
    
    // Script generation helper methods
    std::string processTemplate(const std::string& templateContent, const json& parameters);
    bool validateParameters(const std::vector<std::string>& required, const json& provided);
    json parseJsonScript(const std::string& script);
    json parseYamlScript(const std::string& script);
    std::string generateJsonScript(const json& data);
    std::string generateYamlScript(const json& data);
    std::vector<std::string> validateScriptStructure(const json& script);
    json optimizeScriptJson(const json& script);
};

// Regex patterns are optimized at compile time
const Regex TaskGenerator::Impl::MACRO_PATTERN(
    R"(\$\{([^\{\}]+(?:$[^\{\}]*$)*)\})", Regex::optimize);
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

        std::string result;
        size_t total_size = 0;
        for (const auto& arg : args) {
            total_size += arg.size() + 1;  // +1 for potential space
        }
        result.reserve(total_size);

        result = args[0];
        for (size_t i = 1; i < args.size(); ++i) {
            if (!args[i].empty()) {
                if (std::ispunct(args[i][0]) && args[i][0] != '(' &&
                    args[i][0] != '[') {
                    result.append(args[i]);
                } else {
                    result.append(" ").append(args[i]);
                }
            }
        }
        return result;
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
    spdlog::info("Adding macro: {}", name);

    std::unique_lock lock(mutex_);
    macros_[name] = std::move(value);

    std::unique_lock cacheLock(cache_mutex_);
    macro_cache_.clear();
    spdlog::info("Cache cleared after adding macro: {}", name);
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
        keys.push_back(key);
    }
    return keys;
}

void TaskGenerator::Impl::processJson(json& json_obj) {
    try {
        if (json_obj.is_object()) {
            for (auto& [key, value] : json_obj.items()) {
                if (value.is_string()) {
                    json_obj[key] = replaceMacros(value.get<std::string>());
                } else if (value.is_object() || value.is_array()) {
                    processJson(value);
                }
            }
        } else if (json_obj.is_array()) {
            for (auto& item : json_obj) {
                if (item.is_string()) {
                    item = replaceMacros(item.get<std::string>());
                } else if (item.is_object() || item.is_array()) {
                    processJson(item);
                }
            }
        }
    } catch (const TaskGeneratorException& e) {
        spdlog::error("Error processing JSON: {}", e.what());
        throw;
    }
}

void TaskGenerator::Impl::processJsonWithJsonMacros(json& json_obj) {
    try {
        preprocessJsonMacros(json_obj);  // Preprocess macros
        processJson(json_obj);           // Replace macros in JSON
    } catch (const TaskGeneratorException& e) {
        spdlog::error("Error processing JSON with macros: {}", e.what());
        throw;
    }
}

auto TaskGenerator::Impl::replaceMacros(const std::string& input)
    -> std::string {
    auto start_time = std::chrono::high_resolution_clock::now();
    std::string result = input;
    Match match;

    try {
        std::string::size_type search_pos = 0;
        while (std::regex_search(result.cbegin() + search_pos, result.cend(),
                                 match, MACRO_PATTERN)) {
            std::string macroContent = match[1].str();
            auto match_pos = match.position(0) + search_pos;
            auto match_len = match.length(0);

            std::string replacement;
            {
                std::shared_lock cacheLock(cache_mutex_);
                auto cacheIt = macro_cache_.find(macroContent);
                if (cacheIt != macro_cache_.end()) {
                    ++stats_.cache_hits;
                    replacement = cacheIt->second;
                } else {
                    cacheLock.unlock();
                    ++stats_.cache_misses;

                    replacement = [&]() -> std::string {
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
                                        "Malformed macro definition: " +
                                            macroContent);
                                }
                                macroName = macroContent.substr(0, pos);
                                std::string argsStr = macroContent.substr(
                                    pos + 1, macroContent.length() - pos - 2);

                                std::sregex_token_iterator iter(argsStr.begin(),
                                                                argsStr.end(),
                                                                ARG_PATTERN);
                                std::sregex_token_iterator end;
                                for (; iter != end; ++iter) {
                                    args.push_back(
                                        atom::utils::trim(iter->str()));
                                }
                            }

                            return evaluateMacro(macroName, args);
                        } catch (const TaskGeneratorException& e) {
                            spdlog::error("Error evaluating macro '{}': {}",
                                          macroContent, e.what());
                            throw;
                        }
                    }();

                    std::unique_lock writeLock(cache_mutex_);
                    macro_cache_[macroContent] = replacement;
                    trimCache();
                }
            }

            result.replace(match_pos, match_len, replacement);
            search_pos = match_pos + replacement.length();
        }
    } catch (const std::exception& e) {
        spdlog::error("Error in macro replacement: {}", e.what());
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
    const std::string& name, const std::vector<std::string>& args) const
    -> std::string {
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
        if (json_obj.is_object()) {
            for (auto& [key, value] : json_obj.items()) {
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
                                    "Malformed macro definition: " +
                                        macroContent);
                            }
                            macroName = macroContent.substr(0, pos);
                            std::string argsStr = macroContent.substr(
                                pos + 1, macroContent.length() - pos - 2);

                            std::sregex_token_iterator iter(
                                argsStr.begin(), argsStr.end(), ARG_PATTERN);
                            std::sregex_token_iterator end;
                            for (; iter != end; ++iter) {
                                args.push_back(atom::utils::trim(iter->str()));
                            }
                        }

                        // Define macro if not already present
                        {
                            std::unique_lock lock(mutex_);
                            if (macros_.find(key) == macros_.end()) {
                                if (args.empty()) {
                                    macros_[key] = macroContent;
                                } else {
                                    macros_[key] = "macro_defined_in_json";
                                }
                            }
                        }

                        spdlog::info("Preprocessed macro: {} -> {}", key,
                                     macroContent);
                    }
                } else if (value.is_object() || value.is_array()) {
                    preprocessJsonMacros(value);
                }
            }
        } else if (json_obj.is_array()) {
            for (auto& item : json_obj) {
                if (item.is_object() || item.is_array()) {
                    preprocessJsonMacros(item);
                }
            }
        }
    } catch (const TaskGeneratorException& e) {
        spdlog::error("Error preprocessing JSON macros: {}", e.what());
        throw;
    }
}

void TaskGenerator::Impl::trimCache() {
    if (macro_cache_.size() > max_cache_size_) {
        // Simple LRU-like approach: remove oldest entries
        size_t to_remove = macro_cache_.size() - max_cache_size_;
        auto it = macro_cache_.begin();
        for (size_t i = 0; i < to_remove; ++i) {
            it = macro_cache_.erase(it);
        }
    }
}

void TaskGenerator::Impl::clearMacroCache() {
    std::unique_lock cacheLock(cache_mutex_);
    macro_cache_.clear();
    spdlog::info("Macro cache cleared");
}

bool TaskGenerator::Impl::hasMacro(const std::string& name) const {
    std::shared_lock lock(mutex_);
    return macros_.find(name) != macros_.end();
}

size_t TaskGenerator::Impl::getCacheSize() const {
    std::shared_lock cacheLock(cache_mutex_);
    return macro_cache_.size();
}

void TaskGenerator::Impl::setMaxCacheSize(size_t size) {
    std::unique_lock lock(cache_mutex_);
    max_cache_size_ = size;
    trimCache();
}

void TaskGenerator::Impl::resetStatistics() {
    stats_ = Statistics{};
    spdlog::info("Statistics reset");
}

// Script generation implementations
void TaskGenerator::Impl::setScriptConfig(const TaskGenerator::ScriptConfig& config) {
    std::unique_lock lock(scriptMutex_);
    scriptConfig_ = config;
    spdlog::info("Script configuration updated");
}

TaskGenerator::ScriptConfig TaskGenerator::Impl::getScriptConfig() const {
    std::shared_lock lock(scriptMutex_);
    return scriptConfig_;
}

void TaskGenerator::Impl::registerScriptTemplate(const std::string& templateName, const TaskGenerator::ScriptTemplate& templateInfo) {
    std::unique_lock lock(scriptMutex_);
    scriptTemplates_[templateName] = templateInfo;
    spdlog::info("Registered script template: {}", templateName);
}

void TaskGenerator::Impl::unregisterScriptTemplate(const std::string& templateName) {
    std::unique_lock lock(scriptMutex_);
    auto it = scriptTemplates_.find(templateName);
    if (it != scriptTemplates_.end()) {
        scriptTemplates_.erase(it);
        spdlog::info("Unregistered script template: {}", templateName);
    } else {
        spdlog::warn("Script template not found: {}", templateName);
    }
}

TaskGenerator::ScriptGenerationResult TaskGenerator::Impl::generateScript(const std::string& templateName, const json& parameters) {
    std::shared_lock lock(scriptMutex_);
    
    TaskGenerator::ScriptGenerationResult result;
    
    auto it = scriptTemplates_.find(templateName);
    if (it == scriptTemplates_.end()) {
        result.errors.push_back("Template not found: " + templateName);
        return result;
    }
    
    const auto& templateInfo = it->second;
    
    // Validate required parameters
    if (!validateParameters(templateInfo.requiredParams, parameters)) {
        result.errors.push_back("Missing required parameters");
        return result;
    }
    
    try {
        // Process template with macro replacement
        result.generatedScript = processTemplate(templateInfo.content, parameters);
        
        // Add metadata
        result.metadata["template_name"] = templateName;
        result.metadata["template_version"] = templateInfo.version;
        result.metadata["generated_at"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        result.success = true;
        spdlog::info("Generated script from template: {}", templateName);
        
    } catch (const std::exception& e) {
        result.errors.push_back("Script generation failed: " + std::string(e.what()));
        spdlog::error("Script generation failed for template {}: {}", templateName, e.what());
    }
    
    return result;
}

TaskGenerator::ScriptGenerationResult TaskGenerator::Impl::generateSequenceScript(const json& sequenceConfig) {
    TaskGenerator::ScriptGenerationResult result;
    
    try {
        if (!sequenceConfig.contains("targets") || !sequenceConfig["targets"].is_array()) {
            result.errors.push_back("Sequence configuration must contain 'targets' array");
            return result;
        }
        
        json sequence;
        sequence["name"] = sequenceConfig.value("name", "Generated Sequence");
        sequence["description"] = sequenceConfig.value("description", "Auto-generated sequence");
        sequence["targets"] = json::array();
        
        for (const auto& target : sequenceConfig["targets"]) {
            json targetJson;
            targetJson["name"] = target.value("name", "Unnamed Target");
            targetJson["ra"] = target.value("ra", 0.0);
            targetJson["dec"] = target.value("dec", 0.0);
            targetJson["tasks"] = target.value("tasks", json::array());
            
            sequence["targets"].push_back(targetJson);
        }
        
        if (scriptConfig_.outputFormat == "yaml") {
            result.generatedScript = generateYamlScript(sequence);
        } else {
            result.generatedScript = generateJsonScript(sequence);
        }
        
        result.metadata["type"] = "sequence";
        result.metadata["target_count"] = sequenceConfig["targets"].size();
        result.success = true;
        
    } catch (const std::exception& e) {
        result.errors.push_back("Sequence generation failed: " + std::string(e.what()));
    }
    
    return result;
}

TaskGenerator::ScriptGenerationResult TaskGenerator::Impl::parseScript(const std::string& script, const std::string& format) {
    TaskGenerator::ScriptGenerationResult result;
    
    try {
        json parsedScript;
        
        if (format == "yaml") {
            parsedScript = parseYamlScript(script);
        } else {
            parsedScript = parseJsonScript(script);
        }
        
        // Validate structure
        auto errors = validateScriptStructure(parsedScript);
        result.errors = errors;
        
        if (errors.empty()) {
            result.generatedScript = script; // Original script is valid
            result.metadata = parsedScript.value("metadata", json::object());
            result.success = true;
        } else {
            result.success = false;
        }
        
    } catch (const std::exception& e) {
        result.errors.push_back("Parse error: " + std::string(e.what()));
        result.success = false;
    }
    
    return result;
}

std::vector<std::string> TaskGenerator::Impl::getAvailableTemplates() const {
    std::shared_lock lock(scriptMutex_);
    
    std::vector<std::string> templates;
    templates.reserve(scriptTemplates_.size());
    
    for (const auto& [name, _] : scriptTemplates_) {
        templates.push_back(name);
    }
    
    std::sort(templates.begin(), templates.end());
    return templates;
}

std::optional<TaskGenerator::ScriptTemplate> TaskGenerator::Impl::getTemplateInfo(const std::string& templateName) const {
    std::shared_lock lock(scriptMutex_);
    
    auto it = scriptTemplates_.find(templateName);
    if (it != scriptTemplates_.end()) {
        return it->second;
    }
    
    return std::nullopt;
}

TaskGenerator::ScriptGenerationResult TaskGenerator::Impl::generateCustomTaskScript(const std::string& taskType, const json& taskConfig) {
    TaskGenerator::ScriptGenerationResult result;
    
    try {
        json taskScript;
        taskScript["task_type"] = taskType;
        taskScript["name"] = taskConfig.value("name", "Custom Task");
        taskScript["parameters"] = taskConfig.value("parameters", json::object());
        taskScript["timeout"] = taskConfig.value("timeout", 30);
        taskScript["retry_count"] = taskConfig.value("retry_count", 0);
        
        result.generatedScript = generateJsonScript(taskScript);
        result.metadata["task_type"] = taskType;
        result.success = true;
        
    } catch (const std::exception& e) {
        result.errors.push_back("Custom task script generation failed: " + std::string(e.what()));
    }
    
    return result;
}

TaskGenerator::ScriptGenerationResult TaskGenerator::Impl::optimizeScript(const std::string& script) {
    TaskGenerator::ScriptGenerationResult result;
    
    try {
        auto parsedScript = parseJsonScript(script);
        auto optimizedScript = optimizeScriptJson(parsedScript);
        
        result.generatedScript = generateJsonScript(optimizedScript);
        result.metadata["optimized"] = true;
        result.success = true;
        
    } catch (const std::exception& e) {
        result.errors.push_back("Script optimization failed: " + std::string(e.what()));
    }
    
    return result;
}

bool TaskGenerator::Impl::validateScript(const std::string& script, const std::string& templateName) {
    try {
        auto parsedScript = parseJsonScript(script);
        auto errors = validateScriptStructure(parsedScript);
        
        if (!templateName.empty()) {
            std::shared_lock lock(scriptMutex_);
            auto it = scriptTemplates_.find(templateName);
            if (it != scriptTemplates_.end()) {
                // Additional template-specific validation
                const auto& templateInfo = it->second;
                if (!validateParameters(templateInfo.requiredParams, parsedScript)) {
                    return false;
                }
            }
        }
        
        return errors.empty();
        
    } catch (const std::exception&) {
        return false;
    }
}

size_t TaskGenerator::Impl::loadTemplatesFromDirectory(const std::string& templateDir) {
    // Implementation would load template files from directory
    // For now, return 0 as placeholder
    spdlog::info("Loading templates from directory: {}", templateDir);
    return 0;
}

bool TaskGenerator::Impl::saveTemplatesToDirectory(const std::string& templateDir) const {
    // Implementation would save templates to directory
    // For now, return true as placeholder
    spdlog::info("Saving templates to directory: {}", templateDir);
    return true;
}

TaskGenerator::ScriptGenerationResult TaskGenerator::Impl::convertScriptFormat(const std::string& script, 
                                                                               const std::string& fromFormat,
                                                                               const std::string& toFormat) {
    TaskGenerator::ScriptGenerationResult result;
    
    try {
        json parsedScript;
        
        if (fromFormat == "yaml") {
            parsedScript = parseYamlScript(script);
        } else {
            parsedScript = parseJsonScript(script);
        }
        
        if (toFormat == "yaml") {
            result.generatedScript = generateYamlScript(parsedScript);
        } else {
            result.generatedScript = generateJsonScript(parsedScript);
        }
        
        result.metadata["converted_from"] = fromFormat;
        result.metadata["converted_to"] = toFormat;
        result.success = true;
        
    } catch (const std::exception& e) {
        result.errors.push_back("Format conversion failed: " + std::string(e.what()));
    }
    
    return result;
}

// Helper method implementations
std::string TaskGenerator::Impl::processTemplate(const std::string& templateContent, const json& parameters) {
    std::string result = templateContent;
    
    // Replace template variables with parameter values
    for (const auto& [key, value] : parameters.items()) {
        std::string placeholder = "${" + key + "}";
        std::string replacement = value.is_string() ? value.get<std::string>() : value.dump();
        
        size_t pos = 0;
        while ((pos = result.find(placeholder, pos)) != std::string::npos) {
            result.replace(pos, placeholder.length(), replacement);
            pos += replacement.length();
        }
    }
    
    // Apply macro processing
    result = replaceMacros(result);
    
    return result;
}

bool TaskGenerator::Impl::validateParameters(const std::vector<std::string>& required, const json& provided) {
    for (const auto& param : required) {
        if (!provided.contains(param)) {
            return false;
        }
    }
    return true;
}

json TaskGenerator::Impl::parseJsonScript(const std::string& script) {
    return json::parse(script);
}

json TaskGenerator::Impl::parseYamlScript(const std::string& script) {
    // For now, assume JSON format since YAML parsing would require additional library
    // In a real implementation, this would use a YAML parser
    return json::parse(script);
}

std::string TaskGenerator::Impl::generateJsonScript(const json& data) {
    return data.dump(2); // Pretty print with 2-space indentation
}

std::string TaskGenerator::Impl::generateYamlScript(const json& data) {
    // For now, return JSON format since YAML generation would require additional library
    // In a real implementation, this would convert to YAML
    return data.dump(2);
}

std::vector<std::string> TaskGenerator::Impl::validateScriptStructure(const json& script) {
    std::vector<std::string> errors;
    
    // Basic structure validation
    if (!script.is_object()) {
        errors.push_back("Script must be a JSON object");
        return errors;
    }
    
    // Check for required fields based on script type
    if (script.contains("targets") && script["targets"].is_array()) {
        // Sequence script validation
        for (const auto& target : script["targets"]) {
            if (!target.contains("name")) {
                errors.push_back("Target must have a name");
            }
        }
    }
    
    return errors;
}

json TaskGenerator::Impl::optimizeScriptJson(const json& script) {
    json optimized = script;
    
    // Remove unnecessary fields, combine similar tasks, etc.
    // For now, just return the original script
    
    return optimized;
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

void TaskGenerator::clearMacroCache() { impl_->clearMacroCache(); }

bool TaskGenerator::hasMacro(const std::string& name) const {
    return impl_->hasMacro(name);
}

size_t TaskGenerator::getCacheSize() const { return impl_->getCacheSize(); }

void TaskGenerator::setMaxCacheSize(size_t size) {
    impl_->setMaxCacheSize(size);
}

TaskGenerator::Statistics TaskGenerator::getStatistics() const {
    return impl_->stats_;
}

void TaskGenerator::resetStatistics() { impl_->resetStatistics(); }

// Script generation method implementations
void TaskGenerator::setScriptConfig(const ScriptConfig& config) {
    impl_->setScriptConfig(config);
}

TaskGenerator::ScriptConfig TaskGenerator::getScriptConfig() const {
    return impl_->getScriptConfig();
}

void TaskGenerator::registerScriptTemplate(const std::string& templateName, const ScriptTemplate& templateInfo) {
    impl_->registerScriptTemplate(templateName, templateInfo);
}

void TaskGenerator::unregisterScriptTemplate(const std::string& templateName) {
    impl_->unregisterScriptTemplate(templateName);
}

TaskGenerator::ScriptGenerationResult TaskGenerator::generateScript(const std::string& templateName, const json& parameters) {
    return impl_->generateScript(templateName, parameters);
}

TaskGenerator::ScriptGenerationResult TaskGenerator::generateSequenceScript(const json& sequenceConfig) {
    return impl_->generateSequenceScript(sequenceConfig);
}

TaskGenerator::ScriptGenerationResult TaskGenerator::parseScript(const std::string& script, const std::string& format) {
    return impl_->parseScript(script, format);
}

std::vector<std::string> TaskGenerator::getAvailableTemplates() const {
    return impl_->getAvailableTemplates();
}

std::optional<TaskGenerator::ScriptTemplate> TaskGenerator::getTemplateInfo(const std::string& templateName) const {
    return impl_->getTemplateInfo(templateName);
}

TaskGenerator::ScriptGenerationResult TaskGenerator::generateCustomTaskScript(const std::string& taskType, const json& taskConfig) {
    return impl_->generateCustomTaskScript(taskType, taskConfig);
}

TaskGenerator::ScriptGenerationResult TaskGenerator::optimizeScript(const std::string& script) {
    return impl_->optimizeScript(script);
}

bool TaskGenerator::validateScript(const std::string& script, const std::string& templateName) {
    return impl_->validateScript(script, templateName);
}

size_t TaskGenerator::loadTemplatesFromDirectory(const std::string& templateDir) {
    return impl_->loadTemplatesFromDirectory(templateDir);
}

bool TaskGenerator::saveTemplatesToDirectory(const std::string& templateDir) const {
    return impl_->saveTemplatesToDirectory(templateDir);
}

TaskGenerator::ScriptGenerationResult TaskGenerator::convertScriptFormat(const std::string& script, 
                                                                         const std::string& fromFormat,
                                                                         const std::string& toFormat) {
    return impl_->convertScriptFormat(script, fromFormat, toFormat);
}

}  // namespace lithium