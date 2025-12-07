/*
 * logger_registry.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "logger_registry.hpp"

#include <algorithm>

namespace lithium::logging {

auto LoggerRegistry::getOrCreate(const std::string& name,
                                 const std::vector<spdlog::sink_ptr>& sinks,
                                 spdlog::level::level_enum default_level,
                                 const std::string& default_pattern)
    -> std::shared_ptr<spdlog::logger> {
    std::unique_lock lock(mutex_);

    auto logger = spdlog::get(name);
    if (logger) {
        return logger;
    }

    logger = std::make_shared<spdlog::logger>(name, sinks.begin(), sinks.end());
    logger->set_level(default_level);
    logger->set_pattern(default_pattern);

    spdlog::register_logger(logger);
    patterns_[name] = default_pattern;

    return logger;
}

auto LoggerRegistry::get(const std::string& name) const
    -> std::shared_ptr<spdlog::logger> {
    std::shared_lock lock(mutex_);
    return spdlog::get(name);
}

auto LoggerRegistry::exists(const std::string& name) const -> bool {
    std::shared_lock lock(mutex_);
    return spdlog::get(name) != nullptr;
}

auto LoggerRegistry::remove(const std::string& name) -> bool {
    std::unique_lock lock(mutex_);

    if (name == "default" || name.empty()) {
        return false;  // Cannot remove default logger
    }

    spdlog::drop(name);
    patterns_.erase(name);
    return true;
}

auto LoggerRegistry::list() const -> std::vector<LoggerInfo> {
    std::shared_lock lock(mutex_);

    std::vector<LoggerInfo> result;

    spdlog::apply_all([&result, this](std::shared_ptr<spdlog::logger> logger) {
        LoggerInfo info;
        info.name = logger->name();
        info.level = logger->level();

        auto it = patterns_.find(info.name);
        info.pattern = (it != patterns_.end()) ? it->second : "";

        result.push_back(info);
    });

    return result;
}

auto LoggerRegistry::setLevel(const std::string& name,
                              spdlog::level::level_enum level) -> bool {
    std::unique_lock lock(mutex_);

    auto logger = spdlog::get(name);
    if (!logger) {
        return false;
    }

    logger->set_level(level);
    return true;
}

void LoggerRegistry::setGlobalLevel(spdlog::level::level_enum level) {
    std::unique_lock lock(mutex_);
    spdlog::set_level(level);
}

auto LoggerRegistry::setPattern(const std::string& name,
                                const std::string& pattern) -> bool {
    std::unique_lock lock(mutex_);

    auto logger = spdlog::get(name);
    if (!logger) {
        return false;
    }

    logger->set_pattern(pattern);
    patterns_[name] = pattern;
    return true;
}

auto LoggerRegistry::getPattern(const std::string& name) const -> std::string {
    std::shared_lock lock(mutex_);

    auto it = patterns_.find(name);
    return (it != patterns_.end()) ? it->second : "";
}

void LoggerRegistry::addSinkToAll(const spdlog::sink_ptr& sink) {
    std::unique_lock lock(mutex_);

    spdlog::apply_all([&sink](std::shared_ptr<spdlog::logger> logger) {
        logger->sinks().push_back(sink);
    });
}

void LoggerRegistry::removeSinkFromAll(const spdlog::sink_ptr& sink) {
    std::unique_lock lock(mutex_);

    spdlog::apply_all([&sink](std::shared_ptr<spdlog::logger> logger) {
        auto& sinks = logger->sinks();
        sinks.erase(std::remove_if(sinks.begin(), sinks.end(),
                                   [&sink](const spdlog::sink_ptr& s) {
                                       return s.get() == sink.get();
                                   }),
                    sinks.end());
    });
}

void LoggerRegistry::flushAll() {
    std::shared_lock lock(mutex_);

    spdlog::apply_all(
        [](std::shared_ptr<spdlog::logger> logger) { logger->flush(); });
}

void LoggerRegistry::clear() {
    std::unique_lock lock(mutex_);

    // Keep default logger
    auto default_logger = spdlog::default_logger();
    spdlog::drop_all();

    if (default_logger) {
        spdlog::set_default_logger(default_logger);
    }

    patterns_.clear();
}

auto LoggerRegistry::count() const -> size_t {
    std::shared_lock lock(mutex_);

    size_t count = 0;
    spdlog::apply_all([&count](std::shared_ptr<spdlog::logger>) { ++count; });
    return count;
}

}  // namespace lithium::logging
