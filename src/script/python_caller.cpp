#include "python_caller.hpp"

#include <memory_resource>
#include <queue>
#include <string>
#include <unordered_map>
#include <vector>

namespace py = pybind11;

namespace lithium {
struct CacheEntry {
    py::object cached_result;                           // 缓存的Python对象
    std::chrono::system_clock::time_point last_access;  // 最后访问时间
    size_t access_count{0};                             // 访问计数

    CacheEntry(py::object result)
        : cached_result(std::move(result)),
          last_access(std::chrono::system_clock::now()),
          access_count(1) {}

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

    Impl() { LOG_F(INFO, "Initializing Python interpreter."); }

    ~Impl() { LOG_F(INFO, "Shutting down Python interpreter."); }

    void loadScript(const std::string& script_name, const std::string& alias) {
        LOG_F(INFO, "Loading script '{}' with alias '{}'.", script_name, alias);
        try {
            scripts_.emplace(alias, py::module::import(script_name.c_str()));
            LOG_F(INFO, "Script '{}' loaded successfully.", script_name);
        } catch (const py::error_already_set& e) {
            LOG_F(ERROR, "Error loading script '{}': {}", script_name,
                  e.what());
            throw std::runtime_error("Failed to import script '" + script_name +
                                     "': " + e.what());
        }
    }

    void unloadScript(const std::string& alias) {
        LOG_F(INFO, "Unloading script with alias '{}'.", alias);
        auto iter = scripts_.find(alias);
        if (iter != scripts_.end()) {
            scripts_.erase(iter);
            LOG_F(INFO, "Script with alias '{}' unloaded successfully.", alias);
        } else {
            LOG_F(WARNING, "Alias '{}' not found.", alias);
            throw std::runtime_error("Alias '" + alias + "' not found.");
        }
    }

    void reloadScript(const std::string& alias) {
        LOG_F(INFO, "Reloading script with alias '{}'.", alias);
        try {
            auto iter = scripts_.find(alias);
            if (iter == scripts_.end()) {
                LOG_F(WARNING, "Alias '{}' not found for reloading.", alias);
                throw std::runtime_error("Alias '" + alias + "' not found.");
            }
            py::module script = iter->second;
            py::module::import("importlib").attr("reload")(script);
            LOG_F(INFO, "Script with alias '{}' reloaded successfully.", alias);
        } catch (const py::error_already_set& e) {
            LOG_F(ERROR, "Error reloading script '{}': {}", alias, e.what());
            throw std::runtime_error("Failed to reload script '" + alias +
                                     "': " + e.what());
        }
    }

    template <typename ReturnType, typename... Args>
    ReturnType callFunction(const std::string& alias,
                            const std::string& function_name, Args... args) {
        LOG_F(INFO, "Calling function '{}' from alias '{}'.", function_name,
              alias);
        try {
            auto iter = scripts_.find(alias);
            if (iter == scripts_.end()) {
                LOG_F(WARNING, "Alias '{}' not found.", alias);
                throw std::runtime_error("Alias '" + alias + "' not found.");
            }
            py::object result =
                iter->second.attr(function_name.c_str())(args...);
            LOG_F(INFO, "Function '{}' called successfully.", function_name);
            return result.cast<ReturnType>();
        } catch (const py::error_already_set& e) {
            LOG_F(ERROR, "Error calling function '{}': {}", function_name,
                  e.what());
            throw std::runtime_error("Error calling function '" +
                                     function_name + "': " + e.what());
        }
    }

    template <typename T>
    T getVariable(const std::string& alias, const std::string& variable_name) {
        LOG_F(INFO, "Getting variable '{}' from alias '{}'.", variable_name,
              alias);
        try {
            auto iter = scripts_.find(alias);
            if (iter == scripts_.end()) {
                LOG_F(WARNING, "Alias '{}' not found.", alias);
                throw std::runtime_error("Alias '" + alias + "' not found.");
            }
            py::object var = iter->second.attr(variable_name.c_str());
            LOG_F(INFO, "Variable '{}' retrieved successfully.", variable_name);
            return var.cast<T>();
        } catch (const py::error_already_set& e) {
            LOG_F(ERROR, "Error getting variable '{}': {}", variable_name,
                  e.what());
            throw std::runtime_error("Error getting variable '" +
                                     variable_name + "': " + e.what());
        }
    }

    void setVariable(const std::string& alias, const std::string& variable_name,
                     const py::object& value) {
        LOG_F(INFO, "Setting variable '{}' in alias '{}'.", variable_name,
              alias);
        try {
            auto iter = scripts_.find(alias);
            if (iter == scripts_.end()) {
                LOG_F(WARNING, "Alias '{}' not found.", alias);
                throw std::runtime_error("Alias '" + alias + "' not found.");
            }
            iter->second.attr(variable_name.c_str()) = value;
            LOG_F(INFO, "Variable '{}' set successfully.", variable_name);
        } catch (const py::error_already_set& e) {
            LOG_F(ERROR, "Error setting variable '{}': {}", variable_name,
                  e.what());
            throw std::runtime_error("Error setting variable '" +
                                     variable_name + "': " + e.what());
        }
    }

    auto getFunctionList(const std::string& alias) -> std::vector<std::string> {
        LOG_F(INFO, "Getting function list from alias '{}'.", alias);
        std::vector<std::string> functions;
        try {
            auto iter = scripts_.find(alias);
            if (iter == scripts_.end()) {
                LOG_F(WARNING, "Alias '{}' not found.", alias);
                throw std::runtime_error("Alias '" + alias + "' not found.");
            }
            py::dict dict = iter->second.attr("__dict__");
            for (auto item : dict) {
                if (py::isinstance<py::function>(item.second)) {
                    functions.emplace_back(py::str(item.first));
                }
            }
            LOG_F(INFO, "Function list retrieved successfully from alias '{}'.",
                  alias);
        } catch (const py::error_already_set& e) {
            LOG_F(ERROR, "Error getting function list: {}", e.what());
            throw std::runtime_error("Error getting function list: " +
                                     std::string(e.what()));
        }
        return functions;
    }

    auto callMethod(const std::string& alias, const std::string& class_name,
                    const std::string& method_name,
                    const py::args& args) -> py::object {
        LOG_F(INFO, "Calling method '{}' of class '{}' from alias '{}'.",
              method_name, class_name, alias);
        try {
            auto iter = scripts_.find(alias);
            if (iter == scripts_.end()) {
                LOG_F(WARNING, "Alias '{}' not found.", alias);
                throw std::runtime_error("Alias '" + alias + "' not found.");
            }
            py::object pyClass = iter->second.attr(class_name.c_str());
            py::object instance = pyClass();
            py::object result = instance.attr(method_name.c_str())(*args);
            LOG_F(INFO, "Method '{}' called successfully.", method_name);
            return result;
        } catch (const py::error_already_set& e) {
            LOG_F(ERROR, "Error calling method '{}': {}", method_name,
                  e.what());
            throw std::runtime_error("Error calling method '" + method_name +
                                     "': " + e.what());
        }
    }

    template <typename T>
    T getObjectAttribute(const std::string& alias,
                         const std::string& class_name,
                         const std::string& attr_name) {
        LOG_F(INFO, "Getting attribute '{}' from class '{}' in alias '{}'.",
              attr_name, class_name, alias);
        try {
            auto iter = scripts_.find(alias);
            if (iter == scripts_.end()) {
                LOG_F(WARNING, "Alias '{}' not found.", alias);
                throw std::runtime_error("Alias '" + alias + "' not found.");
            }
            py::object pyClass = iter->second.attr(class_name.c_str());
            py::object instance = pyClass();
            py::object attr = instance.attr(attr_name.c_str());
            LOG_F(INFO, "Attribute '{}' retrieved successfully.", attr_name);
            return attr.cast<T>();
        } catch (const py::error_already_set& e) {
            LOG_F(ERROR, "Error getting attribute '{}': {}", attr_name,
                  e.what());
            throw std::runtime_error("Error getting attribute '" + attr_name +
                                     "': " + e.what());
        }
    }

    void setObjectAttribute(const std::string& alias,
                            const std::string& class_name,
                            const std::string& attr_name,
                            const py::object& value) {
        LOG_F(INFO, "Setting attribute '{}' of class '{}' in alias '{}'.",
              attr_name, class_name, alias);
        try {
            auto iter = scripts_.find(alias);
            if (iter == scripts_.end()) {
                LOG_F(WARNING, "Alias '{}' not found.", alias);
                throw std::runtime_error("Alias '" + alias + "' not found.");
            }
            py::object pyClass = iter->second.attr(class_name.c_str());
            py::object instance = pyClass();
            instance.attr(attr_name.c_str()) = value;
            LOG_F(INFO, "Attribute '{}' set successfully.", attr_name);
        } catch (const py::error_already_set& e) {
            LOG_F(ERROR, "Error setting attribute '{}': {}", attr_name,
                  e.what());
            throw std::runtime_error("Error setting attribute '" + attr_name +
                                     "': " + e.what());
        }
    }

    auto evalExpression(const std::string& alias,
                        const std::string& expression) -> py::object {
        LOG_F(INFO, "Evaluating expression '{}' in alias '{}'.", expression,
              alias);
        try {
            auto iter = scripts_.find(alias);
            if (iter == scripts_.end()) {
                LOG_F(WARNING, "Alias '{}' not found.", alias);
                throw std::runtime_error("Alias '" + alias + "' not found.");
            }
            py::object result =
                py::eval(expression, iter->second.attr("__dict__"));
            LOG_F(INFO, "Expression '{}' evaluated successfully.", expression);
            return result;
        } catch (const py::error_already_set& e) {
            LOG_F(ERROR, "Error evaluating expression '{}': {}", expression,
                  e.what());
            throw std::runtime_error("Error evaluating expression '" +
                                     expression + "': " + e.what());
        }
    }

    auto callFunctionWithListReturn(
        const std::string& alias, const std::string& function_name,
        const std::vector<int>& input_list) -> std::vector<int> {
        LOG_F(INFO, "Calling function '{}' with list return from alias '{}'.",
              function_name, alias);
        try {
            auto iter = scripts_.find(alias);
            if (iter == scripts_.end()) {
                LOG_F(WARNING, "Alias '{}' not found.", alias);
                throw std::runtime_error("Alias '" + alias + "' not found.");
            }
            py::list pyList = py::cast(input_list);
            py::object result =
                iter->second.attr(function_name.c_str())(pyList);
            if (!py::isinstance<py::list>(result)) {
                LOG_F(ERROR, "Function '{}' did not return a list.",
                      function_name);
                throw std::runtime_error("Function '" + function_name +
                                         "' did not return a list.");
            }
            auto output = result.cast<std::vector<int>>();
            LOG_F(INFO, "Function '{}' called successfully with list return.",
                  function_name);
            return output;
        } catch (const py::error_already_set& e) {
            LOG_F(ERROR, "Error calling function '{}': {}", function_name,
                  e.what());
            throw std::runtime_error("Error calling function '" +
                                     function_name + "': " + e.what());
        }
    }

    auto listScripts() const -> std::vector<std::string> {
        LOG_F(INFO, "Listing all loaded scripts.");
        std::vector<std::string> aliases;
        aliases.reserve(scripts_.size());
        for (const auto& pair : scripts_) {
            aliases.emplace_back(pair.first);
        }
        LOG_F(INFO, "Total scripts loaded: %zu", aliases.size());
        return aliases;
    }

    // 新增: 线程池实现
    void setupThreadPool() {
        if (config_.enable_threading) {
            thread_pool_.resize(config_.thread_pool_size);
            for (auto& thread : thread_pool_) {
                thread = std::thread([this]() { this->threadWorker(); });
            }
        }
    }

    void threadWorker() {
        while (true) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(queue_mutex_);
                condition_.wait(lock, [this] {
                    return stop_flag_ || !task_queue_.empty();
                });

                if (stop_flag_ && task_queue_.empty()) {
                    return;
                }

                task = std::move(task_queue_.front());
                task_queue_.pop();
            }

            try {
                py::gil_scoped_acquire gil;  // 获取GIL
                task();                      // 执行任务
            } catch (const std::exception& e) {
                LOG_F(ERROR, "Thread worker error: {}", e.what());
                if (error_strategy_ == ErrorHandlingStrategy::THROW_EXCEPTION) {
                    throw;
                }
            }
        }
    }

    // 新增: GIL 优化
    void optimizeGIL() {
        if (config_.enable_gil_optimization) {
            py::gil_scoped_release release;
            // 执行不需要 GIL 的操作
        }
    }

    // 新增: 异常处理增强
    template <typename Func>
    auto executeWithErrorHandling(Func&& func) {
        try {
            return func();
        } catch (const py::error_already_set& e) {
            switch (error_strategy_) {
                case ErrorHandlingStrategy::THROW_EXCEPTION:
                    throw;
                case ErrorHandlingStrategy::RETURN_DEFAULT:
                    return typename std::result_of<Func()>::type{};
                case ErrorHandlingStrategy::LOG_AND_CONTINUE:
                    LOG_F(ERROR, "Python error: {}", e.what());
                    return typename std::result_of<Func()>::type{};
            }
        }
    }

    // 新增: 缓存管理实现
    void manageFunctionCache() {
        if (!config_.enable_caching) {
            return;
        }

        // 清理过期缓存
        auto now = std::chrono::system_clock::now();
        for (auto it = function_cache_.begin(); it != function_cache_.end();) {
            if (now - it->second.last_access > cache_timeout_) {
                it = function_cache_.erase(it);
            } else {
                ++it;
            }
        }
    }

    // 新增: 实现基于ranges的批处理
    template <std::ranges::range R>
        requires PythonConvertible<std::ranges::range_value_t<R>>
    void batchProcess(const std::string& alias,
                      const std::string& function_name, R&& range) {
        std::vector<std::future<void>> futures;

        auto chunks =
            std::ranges::views::chunk(range, 1000);  // 每1000个元素一批
        for (const auto& chunk : chunks) {
            futures.push_back(std::async(std::launch::async, [&, chunk] {
                py::gil_scoped_acquire gil;
                auto iter = scripts_.find(alias);
                if (iter != scripts_.end()) {
                    iter->second.attr(function_name.c_str())(chunk);
                }
            }));
        }

        for (auto& future : futures) {
            future.wait();
        }
    }

    // 新增: 实现span接口
    template <typename T>
    void processData(const std::string& alias, const std::string& function_name,
                     std::span<T> data) {
        std::for_each(data.begin(), data.end(), [&](auto& item) {
            py::gil_scoped_acquire gil;
            auto iter = scripts_.find(alias);
            if (iter != scripts_.end()) {
                iter->second.attr(function_name.c_str())(item);
            }
        });
    }

    // 新增: 实现协程支持
    AsyncGenerator<py::object> asyncExecute(const std::string& script) {
        py::gil_scoped_acquire gil;
        py::module_ asyncio = py::module_::import("asyncio");

        while (true) {
            try {
                py::object result = py::eval(script);
                co_yield result;
            } catch (const py::error_already_set& e) {
                break;
            }
        }
    }

    // 新增: 实现线程安全的资源管理
    template <typename T>
    std::shared_ptr<T> getSharedResource(const std::string& resource_name) {
        std::shared_lock read_lock(resource_mutex_);
        if (auto it = resources_.find(resource_name); it != resources_.end()) {
            return std::static_pointer_cast<T>(it->second);
        }

        read_lock.unlock();
        std::unique_lock write_lock(resource_mutex_);

        // 双重检查锁定
        if (auto it = resources_.find(resource_name); it != resources_.end()) {
            return std::static_pointer_cast<T>(it->second);
        }

        auto resource = std::make_shared<T>();
        resources_[resource_name] = resource;
        return resource;
    }

    // 新增: 资源管理
    std::unordered_map<std::string, std::shared_ptr<void>> resources_;
    std::shared_mutex resource_mutex_;

    PerformanceConfig config_;
    ErrorHandlingStrategy error_strategy_ =
        ErrorHandlingStrategy::THROW_EXCEPTION;
    std::vector<std::thread> thread_pool_;
    std::unordered_map<std::string, CacheEntry> function_cache_;
    std::chrono::seconds cache_timeout_{3600};  // 1小时缓存超时
    std::queue<std::function<void()>> task_queue_;
    std::mutex queue_mutex_;
    std::condition_variable condition_;
    bool stop_flag_{false};

    py::scoped_interpreter guard_;
    std::unordered_map<std::string, py::module> scripts_;
};

// 静态成员初始化
thread_local std::pmr::synchronized_pool_resource
    PythonWrapper::Impl::memory_pool;

// PythonWrapper Implementation

PythonWrapper::PythonWrapper() : pImpl(std::make_unique<Impl>()) {}

PythonWrapper::~PythonWrapper() = default;

void PythonWrapper::load_script(const std::string& script_name,
                                const std::string& alias) {
    pImpl->loadScript(script_name, alias);
}

void PythonWrapper::unload_script(const std::string& alias) {
    pImpl->unloadScript(alias);
}

void PythonWrapper::reload_script(const std::string& alias) {
    pImpl->reloadScript(alias);
}

template <typename ReturnType, typename... Args>
auto PythonWrapper::call_function(const std::string& alias,
                                  const std::string& function_name,
                                  Args... args) -> ReturnType {
    return pImpl->callFunction<ReturnType>(alias, function_name, args...);
}

template <typename T>
auto PythonWrapper::get_variable(const std::string& alias,
                                 const std::string& variable_name) -> T {
    return pImpl->getVariable<T>(alias, variable_name);
}

void PythonWrapper::set_variable(const std::string& alias,
                                 const std::string& variable_name,
                                 const py::object& value) {
    pImpl->setVariable(alias, variable_name, value);
}

auto PythonWrapper::get_function_list(const std::string& alias)
    -> std::vector<std::string> {
    return pImpl->getFunctionList(alias);
}

auto PythonWrapper::call_method(const std::string& alias,
                                const std::string& class_name,
                                const std::string& method_name,
                                const py::args& args) -> py::object {
    return pImpl->callMethod(alias, class_name, method_name, args);
}

template <typename T>
auto PythonWrapper::get_object_attribute(const std::string& alias,
                                         const std::string& class_name,
                                         const std::string& attr_name) -> T {
    return pImpl->getObjectAttribute<T>(alias, class_name, attr_name);
}

void PythonWrapper::set_object_attribute(const std::string& alias,
                                         const std::string& class_name,
                                         const std::string& attr_name,
                                         const py::object& value) {
    pImpl->setObjectAttribute(alias, class_name, attr_name, value);
}

auto PythonWrapper::eval_expression(
    const std::string& alias, const std::string& expression) -> py::object {
    return pImpl->evalExpression(alias, expression);
}

auto PythonWrapper::call_function_with_list_return(
    const std::string& alias, const std::string& function_name,
    const std::vector<int>& input_list) -> std::vector<int> {
    return pImpl->callFunctionWithListReturn(alias, function_name, input_list);
}

auto PythonWrapper::list_scripts() const -> std::vector<std::string> {
    return pImpl->listScripts();
}

void PythonWrapper::set_error_handling_strategy(
    ErrorHandlingStrategy strategy) {
    pImpl->error_strategy_ = strategy;
}

void PythonWrapper::configure_performance(const PerformanceConfig& config) {
    pImpl->config_ = config;
    if (config.enable_threading) {
        pImpl->setupThreadPool();
    }
    if (config.enable_gil_optimization) {
        pImpl->optimizeGIL();
    }
}

template <typename ReturnType>
std::future<ReturnType> PythonWrapper::async_call_function(
    const std::string& alias, const std::string& function_name) {
    return std::async(std::launch::async, [this, alias, function_name]() {
        return this->call_function<ReturnType>(alias, function_name);
    });
}

// Explicit template instantiation
template int PythonWrapper::call_function<int>(const std::string&,
                                               const std::string&);
template std::string PythonWrapper::get_variable<std::string>(
    const std::string&, const std::string&);
template int PythonWrapper::get_object_attribute<int>(const std::string&,
                                                      const std::string&,
                                                      const std::string&);
}  // namespace lithium
