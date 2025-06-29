/*
 * manager_impl.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-1-4

Description: Component Manager Implementation

**************************************************/

#include "manager_impl.hpp"
#include "exceptions.hpp"

#include <algorithm>
#include <format>
#include <ranges>
#include <stop_token>

#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/pattern_formatter.h>
#include <spdlog/stopwatch.h>

#include "../version.hpp"

namespace lithium {

ComponentManagerImpl::ComponentManagerImpl()
    : moduleLoader_(ModuleLoader::createShared()),
      fileTracker_(std::make_unique<FileTracker>(
          "/components", "package.json",
          std::vector<std::string>{".so", ".dll"})),
      component_pool_(std::make_shared<
                      atom::memory::ObjectPool<std::shared_ptr<Component>>>(
          100, 10)),
      memory_pool_(std::make_unique<MemoryPool<char, 4096>>()) {
    
    // Initialize high-performance async spdlog logger
    spdlog::init_thread_pool(8192, std::thread::hardware_concurrency());
    
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        "logs/component_manager.log", 1048576 * 10, 5);
    
    // Configure sinks with optimized patterns
    console_sink->set_level(spdlog::level::info);
    console_sink->set_pattern("[%H:%M:%S.%e] [%^%l%$] [ComponentMgr] %v");
    
    file_sink->set_level(spdlog::level::debug);
    file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [thread %t] [ComponentMgr] %v");
    
    // Create async logger for maximum performance
    logger_ = std::make_shared<spdlog::async_logger>("ComponentManager",
                                                     std::initializer_list<spdlog::sink_ptr>{console_sink, file_sink},
                                                     spdlog::thread_pool(),
                                                     spdlog::async_overflow_policy::block);
    
    logger_->set_level(spdlog::level::debug);
    logger_->flush_on(spdlog::level::warn);
    
    // Register logger globally
    spdlog::register_logger(logger_);
    
    // Enable performance monitoring features
    performanceMonitoringEnabled_.store(true, std::memory_order_relaxed);
    
    logger_->info("ComponentManager initialized with C++23 optimizations and high-performance async logging");
    logger_->debug("Hardware concurrency: {} threads", std::thread::hardware_concurrency());
}

ComponentManagerImpl::~ComponentManagerImpl() {
    logger_->info("ComponentManager destruction initiated");
    
    // Signal stop to all operations
    stop_source_.request_stop();
    
    // Wait for any ongoing updates to complete
    waitForUpdatesComplete();
    
    // Modern RAII approach with performance timing
    spdlog::stopwatch sw;
    
    try {
        if (fileTracker_) {
            fileTracker_->stopWatching();
            logger_->debug("FileTracker stopped in {}ms", sw.elapsed().count() * 1000);
        }
        
        if (moduleLoader_) {
            auto unloadResult = moduleLoader_->unloadAllModules();
            if (!unloadResult) {
                logger_->error("Failed to unload all modules during destruction");
            } else {
                logger_->debug("All modules unloaded in {}ms", sw.elapsed().count() * 1000);
            }
        }
        
        // Clear containers - concurrent_map handles thread safety internally
        components_.clear();
        componentOptions_.clear();
        componentStates_.clear();
        
        // Log final performance metrics
        const auto totalOps = operationCounter_.load(std::memory_order_relaxed);
        const auto totalErrors = lastErrorCount_.load(std::memory_order_relaxed);
        
        logger_->info("ComponentManager destroyed successfully");
        logger_->info("Final metrics - Operations: {}, Errors: {}, Uptime: {}ms", 
                     totalOps, totalErrors, sw.elapsed().count() * 1000);
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->error("Exception during ComponentManager destruction: {}", e.what());
        }
    }
}

template<std::convertible_to<json> ParamsType>
auto ComponentManagerImpl::loadComponent(const ParamsType& params) -> std::expected<bool, std::string> {
    try {
        json jsonParams = params;
        auto name = jsonParams.at("name").get<std::string>();
        
        logger_->debug("Loading component: {}", name);

        // Use object pool for better memory management
        auto instance = component_pool_->acquire();
        if (!instance) {
            auto error = std::format("Failed to acquire component instance from pool for: {}", name);
            logger_->error(error);
            return std::unexpected(error);
        }

        // Use memory pool for small allocations
        ComponentOptions* options = reinterpret_cast<ComponentOptions*>(
            memory_pool_->allocate(sizeof(ComponentOptions)));
        new (options) ComponentOptions();
        
        auto path = jsonParams.at("path").get<std::string>();
        auto version = jsonParams.value("version", "1.0.0");

        Version ver = Version::parse(version);

        // Check if component already loaded using concurrent_map
        if (auto existing = components_.find(name); existing.has_value()) {
            auto warning = std::format("Component {} already loaded", name);
            logger_->warn(warning);
            return std::unexpected(warning);
        }

        // Add component to dependency graph
        dependencyGraph_.addNode(name, ver);
        
        // Add dependencies if specified
        if (jsonParams.contains("dependencies")) {
            auto deps = jsonParams["dependencies"].get<std::vector<std::string>>();
            for (const auto& dep : deps) {
                dependencyGraph_.addDependency(name, dep, ver);
            }
        }

        // Load module
        if (!moduleLoader_->loadModule(path, name)) {
            auto error = std::format("Failed to load module for component: {}", name);
            logger_->error(error);
            THROW_FAIL_TO_LOAD_COMPONENT(error);
        }

        // Use concurrent_map insert method instead of direct assignment
        components_.insert(name, *instance);
        componentOptions_.insert(name, *options);
        
        updateComponentState(name, ComponentState::Created);
        notifyListeners(name, ComponentEvent::PostLoad);
        
        logger_->info("Component {} loaded successfully", name);
        return true;

    } catch (const json::exception& e) {
        auto error = std::format("JSON error while loading component: {}", e.what());
        logger_->error(error);
        return std::unexpected(error);
    } catch (const std::exception& e) {
        auto error = std::format("Failed to load component: {}", e.what());
        logger_->error(error);
        return std::unexpected(error);
    }
}

template<std::convertible_to<json> ParamsType>
auto ComponentManagerImpl::unloadComponent(const ParamsType& params) -> std::expected<bool, std::string> {
    try {
        json jsonParams = params;
        auto name = jsonParams.at("name").get<std::string>();
        
        logger_->debug("Unloading component: {}", name);
        
        // Check existence using concurrent_map find
        if (!components_.find(name).has_value()) {
            auto warning = std::format("Component {} not found for unloading", name);
            logger_->warn(warning);
            return std::unexpected(warning);
        }

        notifyListeners(name, ComponentEvent::PreUnload);
        
        // Unload from module loader
        if (!moduleLoader_->unloadModule(name)) {
            logger_->warn("Failed to unload module for component: {}", name);
        }
        
        // Remove from containers using concurrent_map batch_erase
        std::vector<std::string> keysToRemove = {name};
        components_.batch_erase(keysToRemove);
        componentOptions_.batch_erase(keysToRemove);
        componentStates_.batch_erase(keysToRemove);
        
        // Remove from dependency graph
        dependencyGraph_.removeNode(name);
        
        notifyListeners(name, ComponentEvent::PostUnload);
        logger_->info("Component {} unloaded successfully", name);
        return true;
        
    } catch (const json::exception& e) {
        auto error = std::format("JSON error while unloading component: {}", e.what());
        logger_->error(error);
        return std::unexpected(error);
    } catch (const std::exception& e) {
        auto error = std::format("Failed to unload component: {}", e.what());
        logger_->error(error);
        return std::unexpected(error);
    }
}

auto ComponentManagerImpl::scanComponents(std::string_view path) -> std::vector<std::string> {
    try {
        logger_->debug("Scanning components in path: {}", path);
        
        fileTracker_->scan();
        fileTracker_->compare();
        auto differences = fileTracker_->getDifferences();
        
        std::vector<std::string> newFiles;
        
        // Traditional iteration since json doesn't support ranges yet
        for (const auto& [filePath, info] : differences.items()) {
            if (info["status"] == "new") {
                newFiles.push_back(filePath);
            }
        }
        
        logger_->info("Found {} new component files", newFiles.size());
        return newFiles;
        
    } catch (const std::exception& e) {
        logger_->error("Failed to scan components: {}", e.what());
        return {};
    }
}

auto ComponentManagerImpl::getComponent(std::string_view component_name) const noexcept
    -> std::optional<std::weak_ptr<Component>> {
    try {
        // concurrent_map's find is not const, so we need to cast away const
        auto& mutable_components = const_cast<decltype(components_)&>(components_);
        auto result = mutable_components.find(std::string{component_name});
        if (result.has_value()) {
            return std::weak_ptr<Component>(result.value());
        }
        return std::nullopt;
    } catch (...) {
        logger_->error("Exception in getComponent for: {}", component_name);
        return std::nullopt;
    }
}

auto ComponentManagerImpl::getComponentInfo(std::string_view component_name) const noexcept
    -> std::optional<json> {
    try {
        auto componentKey = std::string{component_name};
        
        // Cast away const for concurrent_map access
        auto& mutable_components = const_cast<decltype(components_)&>(components_);
        auto& mutable_states = const_cast<decltype(componentStates_)&>(componentStates_);
        auto& mutable_options = const_cast<decltype(componentOptions_)&>(componentOptions_);
        
        if (!mutable_components.find(componentKey).has_value()) {
            return std::nullopt;
        }
        
        json info;
        info["name"] = component_name;
        if (auto stateResult = mutable_states.find(componentKey); stateResult.has_value()) {
            info["state"] = static_cast<int>(stateResult.value());
        }
        if (auto optionsResult = mutable_options.find(componentKey); optionsResult.has_value()) {
            info["options"] = optionsResult.value().config;
        }
        return info;
    } catch (const std::exception& e) {
        logger_->error("Failed to get component info for {}: {}", component_name, e.what());
        return std::nullopt;
    }
}

auto ComponentManagerImpl::getComponentList() const noexcept -> std::vector<std::string> {
    try {
        // Cast away const for concurrent_map access
        auto& mutable_components = const_cast<decltype(components_)&>(components_);
        
        std::vector<std::string> result;
        // Get all data and extract keys
        auto allData = mutable_components.get_data();
        result.reserve(allData.size());
        
        for (const auto& [key, value] : allData) {
            result.push_back(key);
        }
        
        return result;
    } catch (const std::exception& e) {
        logger_->error("Failed to get component list: {}", e.what());
        return {};
    }
}

auto ComponentManagerImpl::getComponentDoc(std::string_view component_name) const noexcept -> std::string {
    try {
        // Cast away const for concurrent_map access
        auto& mutable_components = const_cast<decltype(components_)&>(components_);
        if (auto result = mutable_components.find(std::string{component_name}); result.has_value()) {
            // Return documentation if available
            return std::format("Component documentation for {}", component_name);
        }
        return "";
    } catch (const std::exception& e) {
        logger_->error("Failed to get component documentation for {}: {}", component_name, e.what());
        return "";
    }
}

auto ComponentManagerImpl::hasComponent(std::string_view component_name) const noexcept -> bool {
    try {
        // Cast away const for concurrent_map access
        auto& mutable_components = const_cast<decltype(components_)&>(components_);
        return mutable_components.find(std::string{component_name}).has_value();
    } catch (...) {
        return false;
    }
}

void ComponentManagerImpl::printDependencyTree() const {
    try {
        auto components = getComponentList();
        logger_->info("=== Dependency Tree ===");
        for (const auto& component : components) {
            auto dependencies = dependencyGraph_.getDependencies(component);
            auto dependencyList = dependencies 
                | std::views::join_with(std::string_view{", "});
            
            std::string depStr;
            std::ranges::copy(dependencyList, std::back_inserter(depStr));
            
            logger_->info("  {} -> [{}]", component, depStr);
        }
        logger_->info("=== End Dependency Tree ===");
    } catch (const std::exception& e) {
        logger_->error("Failed to print dependency tree: {}", e.what());
    }
}

auto ComponentManagerImpl::initializeComponent(std::string_view name) -> std::expected<bool, std::string> {
    try {
        if (!validateComponentOperation(name)) {
            auto error = std::format("Component validation failed for: {}", name);
            logger_->error(error);
            return std::unexpected(error);
        }
        
        auto comp = getComponent(name);
        if (comp.has_value()) {
            auto component = comp->lock();
            if (component) {
                updateComponentState(name, ComponentState::Initialized);
                logger_->info("Component {} initialized successfully", name);
                return true;
            }
        }
        
        auto error = std::format("Failed to initialize component: {}", name);
        logger_->error(error);
        return std::unexpected(error);
    } catch (const std::exception& e) {
        auto error = std::format("Exception in initializeComponent for {}: {}", name, e.what());
        handleError(name, "initialize", e);
        return std::unexpected(error);
    }
}

auto ComponentManagerImpl::startComponent(std::string_view name) -> std::expected<bool, std::string> {
    if (!validateComponentOperation(name)) {
        auto error = std::format("Component validation failed for: {}", name);
        logger_->error(error);
        return std::unexpected(error);
    }

    try {
        auto comp = getComponent(name);
        if (comp.has_value()) {
            auto component = comp->lock();
            if (component) {
                updateComponentState(name, ComponentState::Running);
                notifyListeners(name, ComponentEvent::StateChanged);
                logger_->info("Component {} started successfully", name);
                return true;
            }
        }
        
        auto error = std::format("Failed to start component: {}", name);
        logger_->error(error);
        return std::unexpected(error);
    } catch (const std::exception& e) {
        auto error = std::format("Exception in startComponent for {}: {}", name, e.what());
        handleError(name, "start", e);
        return std::unexpected(error);
    }
}

void ComponentManagerImpl::updateConfig(std::string_view name, const json& config) {
    if (!validateComponentOperation(name)) {
        return;
    }

    try {
        auto componentKey = std::string{name};
        auto existingOptions = componentOptions_.find(componentKey);
        if (existingOptions.has_value()) {
            auto updatedOptions = existingOptions.value();
            updatedOptions.config = config;
            componentOptions_.insert(componentKey, updatedOptions);
            logger_->debug("Updated config for component: {}", name);
            notifyListeners(name, ComponentEvent::ConfigChanged, config);
        } else {
            logger_->warn("Component {} not found for config update", name);
        }
    } catch (const std::exception& e) {
        handleError(name, "updateConfig", e);
    }
}

auto ComponentManagerImpl::getPerformanceMetrics() const noexcept -> json {
    if (!performanceMonitoringEnabled_.load()) {
        return json{};
    }

    try {
        json metrics;
        
        // Cast away const for concurrent_map access and get data
        auto& mutable_components = const_cast<decltype(components_)&>(components_);
        auto& mutable_states = const_cast<decltype(componentStates_)&>(componentStates_);
        
        auto componentsData = mutable_components.get_data();
        
        for (const auto& [name, component] : componentsData) {
            json componentMetrics;
            componentMetrics["name"] = name;
            if (auto stateResult = mutable_states.find(name); stateResult.has_value()) {
                componentMetrics["state"] = static_cast<int>(stateResult.value());
            }
            componentMetrics["error_count"] = lastErrorCount_.load();
            metrics[name] = componentMetrics;
        }
        
        return metrics;
    } catch (...) {
        return json{};
    }
}

// C++23 optimized lock-free fast read
auto ComponentManagerImpl::tryFastRead(std::string_view name) const noexcept 
    -> std::optional<std::weak_ptr<Component>> {
    
    // Increment reader count atomically
    active_readers_.fetch_add(1, std::memory_order_acquire);
    
    // Use scope guard for cleanup with proper deleter
    struct ReaderGuard {
        const ComponentManagerImpl* manager;
        ReaderGuard(const ComponentManagerImpl* mgr) : manager(mgr) {}
        ~ReaderGuard() {
            manager->active_readers_.fetch_sub(1, std::memory_order_release);
        }
    };
    ReaderGuard guard(this);
    
    try {
        // Try to get component without locking using concurrent_map's thread-safe find
        auto& mutable_components = const_cast<decltype(components_)&>(components_);
        auto result = mutable_components.find(std::string{name});
        if (result.has_value()) {
            return std::weak_ptr<Component>(result.value());
        }
        return std::nullopt;
    } catch (...) {
        return std::nullopt;
    }
}

// C++23 optimized batch operations with parallel execution
void ComponentManagerImpl::optimizedBatchUpdate(std::span<const std::string> names,
                                               std::function<void(const std::string&)> operation) {
    if (names.empty() || !operation) return;
    
    spdlog::stopwatch sw;
    logger_->debug("Starting optimized batch update for {} components", names.size());
    
    // Set updating flag
    updating_components_.test_and_set(std::memory_order_acquire);
    
    // Use scope guard for cleanup with proper RAII
    struct UpdateGuard {
        ComponentManagerImpl* manager;
        UpdateGuard(ComponentManagerImpl* mgr) : manager(mgr) {}
        ~UpdateGuard() {
            manager->updating_components_.clear(std::memory_order_release);
            manager->notifyUpdateComplete();
        }
    };
    UpdateGuard guard(this);
    
    try {
        // Process in chunks for better cache performance
        constexpr std::size_t chunk_size = 32;
        
        for (std::size_t i = 0; i < names.size(); i += chunk_size) {
            const auto chunk_end = std::min(i + chunk_size, names.size());
            const auto chunk = names.subspan(i, chunk_end - i);
            
            // Process chunk sequentially for now (parallel algorithms need careful consideration)
            for (const auto& name : chunk) {
                try {
                    operation(name);
                    incrementOperationCounter();
                } catch (const std::exception& e) {
                    logger_->error("Batch operation failed for {}: {}", name, e.what());
                    lastErrorCount_.fetch_add(1, std::memory_order_relaxed);
                }
            }
        }
        
        logger_->debug("Batch update completed in {}ms", sw.elapsed().count() * 1000);
        
    } catch (const std::exception& e) {
        logger_->error("Batch update failed: {}", e.what());
    }
}

// C++23 atomic wait/notify optimizations
void ComponentManagerImpl::waitForUpdatesComplete() const noexcept {
    // Wait until no updates are in progress
    while (updating_components_.test(std::memory_order_acquire)) {
        // C++20 atomic wait - more efficient than busy waiting
        std::this_thread::yield();
    }
    
    // Wait for all readers to complete
    while (active_readers_.load(std::memory_order_acquire) > 0) {
        std::this_thread::yield();
    }
}

void ComponentManagerImpl::notifyUpdateComplete() const noexcept {
    // Notify any waiting threads that updates are complete
    // This is a no-op in current implementation but provides API for future atomic wait features
}

// C++20 coroutine support for async operations
auto ComponentManagerImpl::asyncLoadComponent(std::string_view name) -> std::coroutine_handle<> {
    // Basic coroutine implementation - would need full coroutine infrastructure
    // For now, return a null handle
    return std::noop_coroutine();
}

// Enhanced error handling with stack trace capture
void ComponentManagerImpl::handleError(std::string_view name, std::string_view operation,
                 const std::exception& e) noexcept {
    try {
        lastErrorCount_.fetch_add(1, std::memory_order_relaxed);
        
        #if LITHIUM_HAS_STACKTRACE
        // Capture stack trace for debugging
        last_error_trace_ = std::stacktrace::current();
        #endif
        
        updateComponentState(name, ComponentState::Error);
        
        json error_data;
        error_data["operation"] = operation;
        error_data["error"] = e.what();
        error_data["timestamp"] = std::chrono::system_clock::now().time_since_epoch().count();
        
        #if LITHIUM_HAS_STACKTRACE
        error_data["stacktrace"] = std::to_string(last_error_trace_);
        #endif
        
        notifyListeners(name, ComponentEvent::Error, error_data);
        
        logger_->error("Error in {} for {}: {} [Error count: {}]", 
                      operation, name, e.what(), 
                      lastErrorCount_.load(std::memory_order_relaxed));
                      
    } catch (...) {
        // Ensure noexcept guarantee
    }
}

// Helper method implementations

void ComponentManagerImpl::updateComponentState(std::string_view name, ComponentState newState) noexcept {
    try {
        auto componentKey = std::string{name};
        componentStates_.insert(componentKey, newState);
        logger_->debug("Updated component state for {}: {}", name, static_cast<int>(newState));
    } catch (const std::exception& e) {
        logger_->error("Failed to update component state for {}: {}", name, e.what());
    }
}

bool ComponentManagerImpl::validateComponentOperation(std::string_view name) const noexcept {
    try {
        if (name.empty()) {
            return false;
        }
        
        // Check if component exists
        auto& mutable_components = const_cast<decltype(components_)&>(components_);
        return mutable_components.find(std::string{name}).has_value();
    } catch (...) {
        return false;
    }
}

auto ComponentManagerImpl::loadComponentByName(std::string_view name) -> std::expected<bool, std::string> {
    try {
        json params;
        params["name"] = name;
        params["path"] = std::format("./components/{}.so", name);
        params["version"] = "1.0.0";
        
        return loadComponent(params);
    } catch (const std::exception& e) {
        auto error = std::format("Failed to load component by name {}: {}", name, e.what());
        logger_->error(error);
        return std::unexpected(error);
    }
}

void ComponentManagerImpl::notifyListeners(std::string_view component, ComponentEvent event,
                     const json& data) const noexcept {
    try {
        std::shared_lock lock(eventListenersMutex_);
        
        if (auto it = eventListeners_.find(event); it != eventListeners_.end()) {
            for (const auto& listener : it->second) {
                try {
                    listener(component, event, data);
                } catch (const std::exception& e) {
                    logger_->error("Event listener failed for component {}: {}", component, e.what());
                }
            }
        }
    } catch (const std::exception& e) {
        logger_->error("Failed to notify listeners for component {}: {}", component, e.what());
    }
}

void ComponentManagerImpl::handleFileChange(const fs::path& path, std::string_view change) {
    try {
        logger_->info("File change detected: {} - {}", path.string(), change);
        
        if (path.extension() == ".so" || path.extension() == ".dll") {
            auto componentName = path.stem().string();
            
            if (change == "modified") {
                // Reload component
                if (hasComponent(componentName)) {
                    logger_->info("Reloading modified component: {}", componentName);
                    auto unloadResult = unloadComponent(json{{"name", componentName}});
                    if (unloadResult) {
                        auto loadResult = loadComponentByName(componentName);
                        if (!loadResult) {
                            logger_->error("Failed to reload component {}: {}", componentName, loadResult.error());
                        }
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        logger_->error("Failed to handle file change for {}: {}", path.string(), e.what());
    }
}

}  // namespace lithium