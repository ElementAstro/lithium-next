#include "python_caller.hpp"

#include <spdlog/spdlog.h>
#include <fstream>
#include <memory_resource>
#include <queue>
#include <string>
#include <unordered_map>
#include <vector>

#include "atom/type/json.hpp"

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

// ============================================================================
// Template Methods Implementation
// ============================================================================

template <typename T>
T PythonWrapper::get_variable(const std::string& alias, const std::string& variable_name) {
    py::gil_scoped_acquire gil;
    auto it = pImpl->scripts_.find(alias);
    if (it == pImpl->scripts_.end()) {
        throw std::runtime_error("Alias '" + alias + "' not found");
    }
    return it->second.attr(variable_name.c_str()).template cast<T>();
}

// Explicit instantiations for get_variable
template int PythonWrapper::get_variable<int>(const std::string&, const std::string&);
template double PythonWrapper::get_variable<double>(const std::string&, const std::string&);
template std::string PythonWrapper::get_variable<std::string>(const std::string&, const std::string&);
template bool PythonWrapper::get_variable<bool>(const std::string&, const std::string&);

void PythonWrapper::set_variable(const std::string& alias,
                                  const std::string& variable_name,
                                  const py::object& value) {
    py::gil_scoped_acquire gil;
    auto it = pImpl->scripts_.find(alias);
    if (it == pImpl->scripts_.end()) {
        throw std::runtime_error("Alias '" + alias + "' not found");
    }
    it->second.attr(variable_name.c_str()) = value;
    spdlog::debug("Set variable '{}' in '{}'", variable_name, alias);
}

template <typename T>
T PythonWrapper::get_object_attribute(const std::string& alias,
                                       const std::string& class_name,
                                       const std::string& attr_name) {
    py::gil_scoped_acquire gil;
    auto it = pImpl->scripts_.find(alias);
    if (it == pImpl->scripts_.end()) {
        throw std::runtime_error("Alias '" + alias + "' not found");
    }
    py::object cls = it->second.attr(class_name.c_str());
    py::object instance = cls();
    return instance.attr(attr_name.c_str()).template cast<T>();
}

// Explicit instantiations for get_object_attribute
template int PythonWrapper::get_object_attribute<int>(const std::string&, const std::string&, const std::string&);
template std::string PythonWrapper::get_object_attribute<std::string>(const std::string&, const std::string&, const std::string&);

void PythonWrapper::set_object_attribute(const std::string& alias,
                                          const std::string& class_name,
                                          const std::string& attr_name,
                                          const py::object& value) {
    py::gil_scoped_acquire gil;
    auto it = pImpl->scripts_.find(alias);
    if (it == pImpl->scripts_.end()) {
        throw std::runtime_error("Alias '" + alias + "' not found");
    }
    py::object cls = it->second.attr(class_name.c_str());
    py::object instance = cls();
    instance.attr(attr_name.c_str()) = value;
}

template <typename T>
py::object PythonWrapper::cpp_to_python(const T& value) {
    py::gil_scoped_acquire gil;
    return py::cast(value);
}

template <typename T>
T PythonWrapper::python_to_cpp(const py::object& obj) {
    py::gil_scoped_acquire gil;
    return obj.cast<T>();
}

// Explicit instantiations for conversion helpers
template py::object PythonWrapper::cpp_to_python<int>(const int&);
template py::object PythonWrapper::cpp_to_python<double>(const double&);
template py::object PythonWrapper::cpp_to_python<std::string>(const std::string&);
template int PythonWrapper::python_to_cpp<int>(const py::object&);
template double PythonWrapper::python_to_cpp<double>(const py::object&);
template std::string PythonWrapper::python_to_cpp<std::string>(const py::object&);

// ============================================================================
// Core Implementation Methods
// ============================================================================

std::vector<std::string> PythonWrapper::list_scripts() const {
    std::vector<std::string> result;
    result.reserve(pImpl->scripts_.size());
    for (const auto& [alias, _] : pImpl->scripts_) {
        result.push_back(alias);
    }
    return result;
}

void PythonWrapper::add_sys_path(const std::string& path) {
    py::gil_scoped_acquire gil;
    py::module_ sys = py::module_::import("sys");
    py::list sys_path = sys.attr("path");
    sys_path.append(path);
    spdlog::debug("Added '{}' to sys.path", path);
}

void PythonWrapper::sync_variable_to_python(const std::string& name, py::object value) {
    py::gil_scoped_acquire gil;
    py::module_ main = py::module_::import("__main__");
    main.attr(name.c_str()) = value;
    spdlog::debug("Synced variable '{}' to Python", name);
}

py::object PythonWrapper::sync_variable_from_python(const std::string& name) {
    py::gil_scoped_acquire gil;
    py::module_ main = py::module_::import("__main__");
    if (py::hasattr(main, name.c_str())) {
        return main.attr(name.c_str());
    }
    return py::none();
}

void PythonWrapper::inject_code(const std::string& code_snippet) {
    py::gil_scoped_acquire gil;
    try {
        py::exec(code_snippet);
        spdlog::debug("Injected code snippet ({} chars)", code_snippet.length());
    } catch (const py::error_already_set& e) {
        spdlog::error("Error injecting code: {}", e.what());
        throw;
    }
}

std::vector<std::string> PythonWrapper::get_function_list(const std::string& alias) {
    py::gil_scoped_acquire gil;
    std::vector<std::string> functions;

    auto it = pImpl->scripts_.find(alias);
    if (it == pImpl->scripts_.end()) {
        throw std::runtime_error("Alias '" + alias + "' not found");
    }

    py::dict moduleDict = it->second.attr("__dict__");
    for (const auto& item : moduleDict) {
        std::string name = py::str(item.first).cast<std::string>();
        if (!name.starts_with("_") && 
            py::isinstance<py::function>(py::reinterpret_borrow<py::object>(item.second))) {
            functions.push_back(name);
        }
    }

    return functions;
}

py::object PythonWrapper::get_memory_usage() {
    py::gil_scoped_acquire gil;
    try {
        py::module_ tracemalloc = py::module_::import("tracemalloc");
        tracemalloc.attr("start")();
        py::tuple snapshot = tracemalloc.attr("get_traced_memory")();
        tracemalloc.attr("stop")();
        return snapshot;
    } catch (const py::error_already_set& e) {
        spdlog::warn("tracemalloc not available: {}", e.what());
        return py::none();
    }
}

void PythonWrapper::register_function(const std::string& name, std::function<void()> func) {
    py::gil_scoped_acquire gil;
    py::module_ main = py::module_::import("__main__");
    main.attr(name.c_str()) = py::cpp_function(func);
    spdlog::debug("Registered C++ function '{}' in Python", name);
}

void PythonWrapper::execute_script_with_logging(const std::string& script_content,
                                                 const std::string& log_file) {
    py::gil_scoped_acquire gil;
    try {
        // Redirect stdout/stderr to file
        std::string setup_code = R"(
import sys
class LogToFile:
    def __init__(self, filename):
        self.file = open(filename, 'a')
        self.original = sys.stdout
    def write(self, text):
        self.file.write(text)
        self.file.flush()
    def flush(self):
        self.file.flush()
    def restore(self):
        sys.stdout = self.original
        self.file.close()

_log_handler = LogToFile(')" + log_file + R"(')
sys.stdout = _log_handler
sys.stderr = _log_handler
)";
        py::exec(setup_code);
        py::exec(script_content);

        // Restore
        py::exec("_log_handler.restore()");

        spdlog::info("Script executed with logging to '{}'", log_file);
    } catch (const py::error_already_set& e) {
        spdlog::error("Error in execute_script_with_logging: {}", e.what());
        throw;
    }
}

void PythonWrapper::execute_with_profiling(const std::string& script_content) {
    py::gil_scoped_acquire gil;
    try {
        py::module_ cProfile = py::module_::import("cProfile");
        py::object profiler = cProfile.attr("Profile")();
        profiler.attr("enable")();
        py::exec(script_content);
        profiler.attr("disable")();
        profiler.attr("print_stats")("cumulative");
    } catch (const py::error_already_set& e) {
        spdlog::error("Profiling error: {}", e.what());
        throw;
    }
}

void PythonWrapper::optimize_memory_usage() {
    py::gil_scoped_acquire gil;
    py::module_ gc = py::module_::import("gc");
    gc.attr("collect")();
    spdlog::debug("Garbage collection performed");
}

void PythonWrapper::clear_unused_resources() {
    optimize_memory_usage();
    pImpl->manage_function_cache();
}

void PythonWrapper::enable_debug_mode(bool enable) {
    py::gil_scoped_acquire gil;
    py::module_ sys = py::module_::import("sys");
    if (enable) {
        sys.attr("settrace")(py::cpp_function([](py::object frame, py::str event, py::object /*arg*/) {
            spdlog::debug("[Python Debug] {} at line {}",
                          event.cast<std::string>(),
                          frame.attr("f_lineno").cast<int>());
            return py::none();
        }));
    } else {
        sys.attr("settrace")(py::none());
    }
}

void PythonWrapper::set_breakpoint(const std::string& alias, int line_number) {
    spdlog::info("Breakpoint set at {}:{}", alias, line_number);
    // Breakpoint handling would require pdb integration
    // This is a placeholder for the feature
}

py::object PythonWrapper::call_method(const std::string& alias,
                                       const std::string& class_name,
                                       const std::string& method_name,
                                       const py::args& args) {
    py::gil_scoped_acquire gil;
    auto it = pImpl->scripts_.find(alias);
    if (it == pImpl->scripts_.end()) {
        throw std::runtime_error("Alias '" + alias + "' not found");
    }

    py::object cls = it->second.attr(class_name.c_str());
    py::object instance = cls();
    return instance.attr(method_name.c_str())(*args);
}

py::object PythonWrapper::eval_expression(const std::string& alias,
                                           const std::string& expression) {
    py::gil_scoped_acquire gil;
    auto it = pImpl->scripts_.find(alias);
    if (it == pImpl->scripts_.end()) {
        throw std::runtime_error("Alias '" + alias + "' not found");
    }

    py::dict local_ns = it->second.attr("__dict__");
    return py::eval(expression, py::globals(), local_ns);
}

std::vector<int> PythonWrapper::call_function_with_list_return(
    const std::string& alias, const std::string& function_name,
    const std::vector<int>& input_list) {
    py::gil_scoped_acquire gil;
    auto it = pImpl->scripts_.find(alias);
    if (it == pImpl->scripts_.end()) {
        throw std::runtime_error("Alias '" + alias + "' not found");
    }

    py::list py_list = py::cast(input_list);
    py::object result = it->second.attr(function_name.c_str())(py_list);
    return result.cast<std::vector<int>>();
}

void PythonWrapper::execute_script_multithreaded(const std::vector<std::string>& scripts) {
    std::vector<std::future<void>> futures;
    futures.reserve(scripts.size());

    for (const auto& script : scripts) {
        futures.push_back(std::async(std::launch::async, [script]() {
            py::gil_scoped_acquire gil;
            try {
                py::exec(script);
            } catch (const py::error_already_set& e) {
                spdlog::error("Multithreaded script error: {}", e.what());
            }
        }));
    }

    for (auto& future : futures) {
        future.get();
    }
}

void PythonWrapper::manage_object_lifecycle(const std::string& alias,
                                            const std::string& object_name,
                                            bool auto_cleanup) {
    // Object lifecycle management is handled automatically by Python's GC
    // This is a placeholder for explicit control if needed
    if (auto_cleanup) {
        spdlog::debug("Object '{}' in '{}' marked for auto-cleanup", object_name, alias);
    }
}

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

// ============================================================================
// Export Discovery Implementation
// ============================================================================

std::optional<ScriptExports> PythonWrapper::discover_exports(
    const std::string& alias) {
    py::gil_scoped_acquire gil;

    auto it = pImpl->scripts_.find(alias);
    if (it == pImpl->scripts_.end()) {
        spdlog::warn("Script '{}' not found for export discovery", alias);
        return std::nullopt;
    }

    ScriptExports exports;
    exports.moduleName = alias;

    try {
        py::module_& module = it->second;

        // Get module file
        if (py::hasattr(module, "__file__")) {
            exports.moduleFile = module.attr("__file__").cast<std::string>();
        }

        // Get module version
        if (py::hasattr(module, "__version__")) {
            exports.version = module.attr("__version__").cast<std::string>();
        }

        // Try to use lithium_bridge.get_export_manifest
        try {
            py::module_ bridge = py::module_::import("lithium_bridge.exporter");
            py::object getManifest = bridge.attr("get_export_manifest");
            py::object manifestObj = getManifest(module);

            py::module_ json = py::module_::import("json");
            std::string jsonStr = json.attr("dumps")(manifestObj).cast<std::string>();

            auto manifest = nlohmann::json::parse(jsonStr);
            exports = ScriptExports::fromJson(manifest);
            exports.moduleName = alias;

            spdlog::info("Discovered {} exports from script '{}'",
                         exports.count(), alias);
            return exports;

        } catch (const py::error_already_set& e) {
            spdlog::debug("lithium_bridge not available: {}", e.what());
        }

        // Fallback: scan for __lithium_exports__ attribute
        py::dict moduleDict = module.attr("__dict__");
        for (const auto& item : moduleDict) {
            std::string name = py::str(item.first).cast<std::string>();
            if (name.starts_with("_")) continue;

            py::object obj = py::reinterpret_borrow<py::object>(item.second);
            if (!py::hasattr(obj, "__lithium_exports__")) continue;

            py::object funcExports = obj.attr("__lithium_exports__");
            if (!py::isinstance<py::list>(funcExports)) continue;

            for (const auto& exportItem : funcExports) {
                if (!py::hasattr(exportItem, "to_dict")) continue;

                py::module_ json = py::module_::import("json");
                py::object dictObj = exportItem.attr("to_dict")();
                std::string jsonStr = json.attr("dumps")(dictObj).cast<std::string>();

                auto exportJson = nlohmann::json::parse(jsonStr);
                ExportInfo info = ExportInfo::fromJson(exportJson);

                if (info.type == ExportType::CONTROLLER) {
                    exports.controllers.push_back(info);
                } else {
                    exports.commands.push_back(info);
                }
            }
        }

        spdlog::info("Discovered {} exports from script '{}' (fallback)",
                     exports.count(), alias);
        return exports;

    } catch (const std::exception& e) {
        spdlog::error("Error discovering exports for '{}': {}", alias, e.what());
        return std::nullopt;
    }
}

std::optional<ScriptExports> PythonWrapper::get_exports(
    const std::string& alias) const {
    // For now, just call discover_exports
    // In a full implementation, we'd cache the results
    return const_cast<PythonWrapper*>(this)->discover_exports(alias);
}

py::object PythonWrapper::invoke_export(const std::string& alias,
                                        const std::string& function_name,
                                        const py::dict& kwargs) {
    py::gil_scoped_acquire gil;

    auto it = pImpl->scripts_.find(alias);
    if (it == pImpl->scripts_.end()) {
        throw std::runtime_error("Script not found: " + alias);
    }

    try {
        py::object func = it->second.attr(function_name.c_str());
        return func(**kwargs);
    } catch (const py::error_already_set& e) {
        spdlog::error("Error invoking '{}' in '{}': {}", function_name, alias,
                      e.what());
        throw;
    }
}

std::optional<std::pair<std::string, ExportInfo>> PythonWrapper::find_by_endpoint(
    const std::string& endpoint) const {
    for (const auto& [alias, module] : pImpl->scripts_) {
        auto exports = get_exports(alias);
        if (!exports) continue;

        for (const auto& ctrl : exports->controllers) {
            if (ctrl.endpoint == endpoint) {
                return std::make_pair(alias, ctrl);
            }
        }
    }
    return std::nullopt;
}

std::optional<std::pair<std::string, ExportInfo>> PythonWrapper::find_by_command(
    const std::string& command_id) const {
    for (const auto& [alias, module] : pImpl->scripts_) {
        auto exports = get_exports(alias);
        if (!exports) continue;

        for (const auto& cmd : exports->commands) {
            if (cmd.commandId == command_id) {
                return std::make_pair(alias, cmd);
            }
        }
    }
    return std::nullopt;
}

std::unordered_map<std::string, ScriptExports> PythonWrapper::get_all_exports()
    const {
    std::unordered_map<std::string, ScriptExports> result;
    for (const auto& [alias, module] : pImpl->scripts_) {
        auto exports = get_exports(alias);
        if (exports) {
            result[alias] = *exports;
        }
    }
    return result;
}

bool PythonWrapper::load_manifest(const std::string& manifest_path,
                                  const std::string& alias) {
    try {
        std::ifstream file(manifest_path);
        if (!file.is_open()) {
            spdlog::error("Failed to open manifest file: {}", manifest_path);
            return false;
        }

        nlohmann::json manifest = nlohmann::json::parse(file);
        auto exports = ScriptExports::fromJson(manifest);
        exports.moduleName = alias;
        exports.moduleFile = manifest_path;

        // Cache the exports (would need to add a cache map to Impl)
        spdlog::info("Loaded manifest for '{}' with {} exports",
                     alias, exports.count());
        return true;

    } catch (const std::exception& e) {
        spdlog::error("Error loading manifest {}: {}", manifest_path, e.what());
        return false;
    }
}

bool PythonWrapper::has_exports(const std::string& alias) const {
    auto exports = get_exports(alias);
    return exports && exports->hasExports();
}

std::vector<std::string> PythonWrapper::list_scripts_with_exports() const {
    std::vector<std::string> result;
    for (const auto& [alias, module] : pImpl->scripts_) {
        if (has_exports(alias)) {
            result.push_back(alias);
        }
    }
    return result;
}

}  // namespace lithium
