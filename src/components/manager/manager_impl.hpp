/*
 * manager_impl.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-1-4

Description: Component Manager Implementation (Private)

**************************************************/

#pragma once

#include <filesystem>
#include <functional>
#include <memory>
#include <shared_mutex>
#include <unordered_map>
#include <concepts>
#include <expected>
#include <string_view>
#include <atomic>
#include <ranges>
#include <future>
#include <format>
#include <stop_token>

#if __has_include(<stacktrace>) && __cpp_lib_stacktrace >= 202011L
#include <stacktrace>
#define LITHIUM_HAS_STACKTRACE 1
#else
#define LITHIUM_HAS_STACKTRACE 0
#endif

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/fmt/ostr.h>
#include <spdlog/async.h>
#include <spdlog/stopwatch.h>

#include "atom/components/component.hpp"
#include "atom/memory/memory.hpp"
#include "atom/memory/object.hpp"
#include "atom/type/json_fwd.hpp"
#include "atom/type/concurrent_map.hpp"

#include "../dependency.hpp"
#include "../loader.hpp"
#include "../tracker.hpp"
#include "types.hpp"

using json = nlohmann::json;

namespace fs = std::filesystem;

namespace lithium {

class ComponentManagerImpl {
public:
    ComponentManagerImpl();
    ~ComponentManagerImpl();

    // C++23 concepts for type safety
    template<std::convertible_to<json> ParamsType>
    auto loadComponent(const ParamsType& params) -> std::expected<bool, std::string>;

    template<std::convertible_to<json> ParamsType>
    auto unloadComponent(const ParamsType& params) -> std::expected<bool, std::string>;

    auto scanComponents(std::string_view path) -> std::vector<std::string>;

    // Modern C++ return types with expected
    auto getComponent(std::string_view component_name) const noexcept
        -> std::optional<std::weak_ptr<Component>>;
    auto getComponentInfo(std::string_view component_name) const noexcept
        -> std::optional<json>;
    auto getComponentList() const noexcept -> std::vector<std::string>;
    auto getComponentDoc(std::string_view component_name) const noexcept -> std::string;
    [[nodiscard]] auto hasComponent(std::string_view component_name) const noexcept -> bool;

    // Range-based operations (C++20) - Implementation
    template<std::ranges::range DepsRange, std::ranges::range VersionsRange>
    void updateDependencyGraph(
        std::string_view component_name, std::string_view version,
        DepsRange&& dependencies,
        VersionsRange&& dependencies_version) {
        try {
            Version ver = Version::parse(std::string{version});
            dependencyGraph_.addNode(std::string{component_name}, ver);

            auto depIter = std::ranges::begin(dependencies);
            auto depVersionIter = std::ranges::begin(dependencies_version);
            auto depEnd = std::ranges::end(dependencies);
            auto depVersionEnd = std::ranges::end(dependencies_version);

            while (depIter != depEnd) {
                Version depVer = (depVersionIter != depVersionEnd)
                               ? Version::parse(std::string{*depVersionIter++})
                               : Version{1, 0, 0};
                dependencyGraph_.addDependency(std::string{component_name}, std::string{*depIter++}, depVer);
            }

            logger_->debug("Updated dependency graph for component: {}", component_name);
        } catch (const std::exception& e) {
            logger_->error("Failed to update dependency graph: {}", e.what());
        }
    }

    template<std::ranges::range ComponentsRange>
    auto batchLoad(ComponentsRange&& components) -> std::expected<bool, std::string> {
        try {
            bool success = true;
            std::vector<std::future<std::expected<bool, std::string>>> futures;

            // Convert range to vector for processing
            std::vector<std::string> componentVec;
            for (auto&& component : components) {
                componentVec.emplace_back(std::forward<decltype(component)>(component));
            }

            // Sort by priority using modern C++ - using find API of concurrent_map
            std::ranges::sort(componentVec, [this](const auto& a, const auto& b) {
                auto optionA = componentOptions_.find(a);
                auto optionB = componentOptions_.find(b);
                int priorityA = optionA.has_value() ? optionA->priority : 0;
                int priorityB = optionB.has_value() ? optionB->priority : 0;
                return priorityA > priorityB;
            });

            // Parallel loading
            for (const auto& name : componentVec) {
                futures.push_back(std::async(std::launch::async, [this, name]() {
                    return loadComponentByName(name);
                }));
            }

            // Wait for all to complete and collect results
            for (auto& future : futures) {
                auto result = future.get();
                if (!result) {
                    success = false;
                    logger_->error("Batch load failed for a component: {}", result.error());
                }
            }

            logger_->info("Batch load completed with success: {}", success);
            return success;
        } catch (const std::exception& e) {
            auto error = std::format("Batch load failed: {}", e.what());
            logger_->error(error);
            return std::unexpected(error);
        }
    }

    void printDependencyTree() const;

    // Component lifecycle operations with expected
    auto initializeComponent(std::string_view name) -> std::expected<bool, std::string>;
    auto startComponent(std::string_view name) -> std::expected<bool, std::string>;
    void updateConfig(std::string_view name, const json& config);
    auto getPerformanceMetrics() const noexcept -> json;

    // Error handling and event system
    void handleError(std::string_view name, std::string_view operation,
                     const std::exception& e) noexcept;
    void notifyListeners(std::string_view component, ComponentEvent event,
                         const json& data = {}) const noexcept;
    void handleFileChange(const fs::path& path, std::string_view change);

private:
    // Core components
    std::shared_ptr<ModuleLoader> moduleLoader_;
    std::unique_ptr<FileTracker> fileTracker_;
    DependencyGraph dependencyGraph_;

    // Component storage with improved concurrency using atom containers
    atom::type::concurrent_map<std::string, std::shared_ptr<Component>> components_;
    atom::type::concurrent_map<std::string, ComponentOptions> componentOptions_;
    atom::type::concurrent_map<std::string, ComponentState> componentStates_;

    // Modern synchronization primitives with C++23 optimizations
    mutable std::shared_mutex eventListenersMutex_;  // Only for event listeners

    // C++20 atomic wait/notify for better lock-free performance
    mutable std::atomic_flag updating_components_ = ATOMIC_FLAG_INIT;
    mutable std::atomic<std::size_t> active_readers_{0};

    // Performance and monitoring with atomics
    std::atomic<bool> performanceMonitoringEnabled_{true};
    mutable std::atomic<std::size_t> lastErrorCount_{0};
    mutable std::atomic<std::size_t> operationCounter_{0};

    // C++23 stop tokens for cancellation
    std::stop_source stop_source_;
    std::stop_token stop_token_{stop_source_.get_token()};

    // Memory management with enhanced pool optimization
    std::shared_ptr<atom::memory::ObjectPool<std::shared_ptr<Component>>>
        component_pool_;
    std::unique_ptr<MemoryPool<char, 4096>> memory_pool_;

    // C++23 stacktrace for better error diagnostics (when available)
    #if LITHIUM_HAS_STACKTRACE
    mutable std::stacktrace last_error_trace_;
    #endif

    // Event system with improved thread safety (using existing mutex)
    std::unordered_map<ComponentEvent,
                       std::vector<std::function<void(
                           std::string_view, ComponentEvent, const json&)>>>
        eventListeners_;

    // Logging
    std::shared_ptr<spdlog::logger> logger_;

    // Helper methods with modern C++ features
    void updateComponentState(std::string_view name, ComponentState newState) noexcept;
    [[nodiscard]] auto validateComponentOperation(std::string_view name) const noexcept -> bool;
    auto loadComponentByName(std::string_view name) -> std::expected<bool, std::string>;

    // C++20 coroutine support for async operations
    auto asyncLoadComponent(std::string_view name) -> std::coroutine_handle<>;

    // C++23 optimized lock-free operations
    [[nodiscard]] auto tryFastRead(std::string_view name) const noexcept
        -> std::optional<std::weak_ptr<Component>>;
    void optimizedBatchUpdate(std::span<const std::string> names,
                             std::function<void(const std::string&)> operation);

    // Lock-free performance counters
    void incrementOperationCounter() noexcept {
        operationCounter_.fetch_add(1, std::memory_order_relaxed);
    }

    // C++23 atomic wait/notify optimizations
    void waitForUpdatesComplete() const noexcept;
    void notifyUpdateComplete() const noexcept;

    // Template constraint helpers
    template<typename T>
    static constexpr bool is_valid_component_name_v =
        std::convertible_to<T, std::string_view> &&
        !std::same_as<std::remove_cvref_t<T>, std::nullptr_t>;
};

}  // namespace lithium
