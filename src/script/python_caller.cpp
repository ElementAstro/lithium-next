#include "python_caller.hpp"

#include <spdlog/spdlog.h>
#include <memory_resource>
#include <queue>
#include <string>
#include <unordered_map>
#include <vector>

namespace py = pybind11;

namespace lithium {

/**
 * @brief Entry in the function call cache
 */
struct CacheEntry {
    py::object cached_result;
    std::chrono::system_clock::time_point last_access;
    size_t access_count{0};

    /**
     * @brief Constructs a new cache entry
     * @param result The result to cache
     */
    explicit CacheEntry(py::object result)
        : cached_result(std::move(result)),
          last_access(std::chrono::system_clock::now()),
          access_count(1) {}

    /**
     * @brief Updates the access time and count
     */
    void update_access() {
        last_access = std::chrono::system_clock::now();
        access_count++;
    }
};

// Implementation class
class PythonWrapper::Impl {
public:
    static thread_local std::pmr::synchronized_pool_resource memory_pool;
    std::vector<std::jthread> worker_threads_;

    Impl() { spdlog::info("Initializing Python interpreter"); }

    ~Impl() { spdlog::info("Shutting down Python interpreter"); }

    /**
     * @brief Loads a Python script
     */
    void load_script(const std::string& script_name, const std::string& alias) {
        spdlog::info("Loading script '{}' with alias '{}'", script_name, alias);
        try {
            scripts_.emplace(alias, py::module::import(script_name.c_str()));
            spdlog::info("Script '{}' loaded successfully", script_name);
        } catch (const py::error_already_set& e) {
            spdlog::error("Error loading script '{}': {}", script_name,
                          e.what());
            throw std::runtime_error("Failed to import script '" + script_name +
                                     "': " + e.what());
        }
    }

    /**
     * @brief Unloads a Python script
     */
    void unload_script(const std::string& alias) {
        spdlog::info("Unloading script with alias '{}'", alias);
        auto iter = scripts_.find(alias);
        if (iter != scripts_.end()) {
            scripts_.erase(iter);
            spdlog::info("Script with alias '{}' unloaded successfully", alias);
        } else {
            spdlog::warn("Alias '{}' not found", alias);
            throw std::runtime_error("Alias '" + alias + "' not found");
        }
    }

    /**
     * @brief Reloads a Python script
     */
    void reload_script(const std::string& alias) {
        spdlog::info("Reloading script with alias '{}'", alias);
        try {
            auto iter = scripts_.find(alias);
            if (iter == scripts_.end()) {
                spdlog::warn("Alias '{}' not found for reloading", alias);
                throw std::runtime_error("Alias '" + alias + "' not found");
            }
            py::module script = iter->second;
            py::module::import("importlib").attr("reload")(script);
            spdlog::info("Script with alias '{}' reloaded successfully", alias);
        } catch (const py::error_already_set& e) {
            spdlog::error("Error reloading script '{}': {}", alias, e.what());
            throw std::runtime_error("Failed to reload script '" + alias +
                                     "': " + e.what());
        }
    }

    /**
     * @brief Calls a Python function
     */
    template <typename ReturnType, typename... Args>
    ReturnType call_function(const std::string& alias,
                             const std::string& function_name, Args... args) {
        spdlog::info("Calling function '{}' from alias '{}'", function_name,
                     alias);
        try {
            auto iter = scripts_.find(alias);
            if (iter == scripts_.end()) {
                spdlog::warn("Alias '{}' not found", alias);
                throw std::runtime_error("Alias '" + alias + "' not found");
            }

            // Check cache if enabled
            if (config_.enable_caching) {
                std::string cache_key = alias + "::" + function_name;
                auto cache_it = function_cache_.find(cache_key);
                if (cache_it != function_cache_.end()) {
                    cache_it->second.update_access();
                    spdlog::debug("Cache hit for function '{}'", function_name);
                    return cache_it->second.cached_result.cast<ReturnType>();
                }
            }

            py::object result =
                iter->second.attr(function_name.c_str())(args...);

            // Store in cache if enabled
            if (config_.enable_caching) {
                std::string cache_key = alias + "::" + function_name;
                function_cache_.emplace(cache_key, CacheEntry(result));
            }

            spdlog::info("Function '{}' called successfully", function_name);
            return result.cast<ReturnType>();
        } catch (const py::error_already_set& e) {
            spdlog::error("Error calling function '{}': {}", function_name,
                          e.what());
            throw std::runtime_error("Error calling function '" +
                                     function_name + "': " + e.what());
        }
    }

    // Other implementation methods...

    /**
     * @brief Sets up the worker thread pool
     */
    void setup_thread_pool() {
        if (config_.enable_threading) {
            worker_threads_.clear();
            worker_threads_.reserve(config_.thread_pool_size);

            for (size_t i = 0; i < config_.thread_pool_size; ++i) {
                worker_threads_.emplace_back(
                    [this](std::stop_token stop_token) {
                        this->thread_worker(stop_token);
                    });
            }

            spdlog::info("Thread pool initialized with {} threads",
                         config_.thread_pool_size);
        }
    }

    /**
     * @brief Worker thread function
     */
    void thread_worker(std::stop_token stop_token) {
        while (!stop_token.stop_requested()) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(queue_mutex_);
                condition_.wait(lock, [this, &stop_token] {
                    return stop_token.stop_requested() || !task_queue_.empty();
                });

                if (stop_token.stop_requested() && task_queue_.empty()) {
                    return;
                }

                if (!task_queue_.empty()) {
                    task = std::move(task_queue_.front());
                    task_queue_.pop();
                } else {
                    continue;
                }
            }

            try {
                py::gil_scoped_acquire gil;  // Acquire the GIL
                task();                      // Execute the task
            } catch (const std::exception& e) {
                spdlog::error("Thread worker error: {}", e.what());
                if (error_strategy_ == ErrorHandlingStrategy::THROW_EXCEPTION) {
                    throw;
                }
            }
        }
    }

    /**
     * @brief Manages the function cache
     */
    void manage_function_cache() {
        if (!config_.enable_caching) {
            return;
        }

        // Clean expired cache entries
        auto now = std::chrono::system_clock::now();
        std::erase_if(function_cache_, [&](const auto& item) {
            return now - item.second.last_access > cache_timeout_;
        });

        spdlog::debug("Cache cleaned, {} entries remaining",
                      function_cache_.size());
    }

    // Performance configuration
    PerformanceConfig config_{.enable_threading = false,
                              .enable_gil_optimization = true,
                              .thread_pool_size = 4,
                              .enable_caching = true};

    ErrorHandlingStrategy error_strategy_ =
        ErrorHandlingStrategy::THROW_EXCEPTION;
    std::vector<std::thread> thread_pool_;
    std::unordered_map<std::string, CacheEntry> function_cache_;
    std::chrono::seconds cache_timeout_{3600};  // 1 hour cache timeout
    std::queue<std::function<void()>> task_queue_;
    std::mutex queue_mutex_;
    std::condition_variable condition_;

    // Python interpreter
    py::scoped_interpreter guard_;
    std::unordered_map<std::string, py::module> scripts_;
};

// Initialize static member
thread_local std::pmr::synchronized_pool_resource
    PythonWrapper::Impl::memory_pool;

// PythonWrapper implementation

PythonWrapper::PythonWrapper() : pImpl(std::make_unique<Impl>()) {}
PythonWrapper::~PythonWrapper() = default;

void PythonWrapper::load_script(const std::string& script_name,
                                const std::string& alias) {
    pImpl->load_script(script_name, alias);
}

void PythonWrapper::unload_script(const std::string& alias) {
    pImpl->unload_script(alias);
}

void PythonWrapper::reload_script(const std::string& alias) {
    pImpl->reload_script(alias);
}

template <typename ReturnType, typename... Args>
ReturnType PythonWrapper::call_function(const std::string& alias,
                                        const std::string& function_name,
                                        Args... args) {
    return pImpl->call_function<ReturnType>(alias, function_name, args...);
}

// Explicitly instantiate common template functions
template int PythonWrapper::call_function<int>(const std::string&,
                                               const std::string&);
template std::string PythonWrapper::call_function<std::string>(
    const std::string&, const std::string&);
template double PythonWrapper::call_function<double>(const std::string&,
                                                     const std::string&);

// Implementation for other methods...

void PythonWrapper::handle_exception(const py::error_already_set& e) {
    spdlog::error("Python Exception: {}", e.what());

    py::module_ traceback = py::module_::import("traceback");
    py::object tb = traceback.attr("format_exc")();
    spdlog::error("Traceback: {}", py::cast<std::string>(tb));
}

void PythonWrapper::set_error_handling_strategy(
    ErrorHandlingStrategy strategy) {
    pImpl->error_strategy_ = strategy;
}

void PythonWrapper::configure_performance(const PerformanceConfig& config) {
    pImpl->config_ = config;

    if (config.enable_threading) {
        pImpl->setup_thread_pool();
    }
}

template <typename ReturnType>
std::future<ReturnType> PythonWrapper::async_call_function(
    const std::string& alias, const std::string& function_name) {
    return std::async(std::launch::async, [this, alias, function_name]() {
        return this->call_function<ReturnType>(alias, function_name);
    });
}

}  // namespace lithium