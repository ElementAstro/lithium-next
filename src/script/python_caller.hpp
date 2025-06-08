#ifndef LITHIUM_SCRIPT_PYTHON_WRAPPER_HPP
#define LITHIUM_SCRIPT_PYTHON_WRAPPER_HPP

#include <pybind11/embed.h>
#include <pybind11/iostream.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <spdlog/spdlog.h>

#include <barrier>
#include <concepts>
#include <coroutine>
#include <future>
#include <latch>
#include <memory>
#include <ranges>
#include <semaphore>
#include <shared_mutex>
#include <span>
#include <string>
#include <vector>

namespace py = pybind11;

namespace lithium {

/**
 * @brief Concept for types that can be converted between C++ and Python
 */
template <typename T>
concept PythonConvertible = requires(T a) {
    { py::cast(a) } -> std::convertible_to<py::object>;
    { py::cast<T>(py::object()) } -> std::same_as<T>;
};

/**
 * @class PythonWrapper
 * @brief A wrapper class to manage and interact with Python scripts.
 */
class PythonWrapper {
public:
    /**
     * @brief Constructs a new PythonWrapper object.
     */
    PythonWrapper();

    /**
     * @brief Destroys the PythonWrapper object.
     */
    ~PythonWrapper();

    // Disable copy
    PythonWrapper(const PythonWrapper&) = delete;
    PythonWrapper& operator=(const PythonWrapper&) = delete;

    /**
     * @brief Loads a Python script and assigns it an alias.
     * @param script_name The name of the Python script to load.
     * @param alias The alias to assign to the loaded script.
     */
    void load_script(const std::string& script_name, const std::string& alias);

    /**
     * @brief Unloads a Python script by its alias.
     * @param alias The alias of the script to unload.
     */
    void unload_script(const std::string& alias);

    /**
     * @brief Reloads a Python script by its alias.
     * @param alias The alias of the script to reload.
     */
    void reload_script(const std::string& alias);

    /**
     * @brief Calls a function in a loaded Python script.
     * @tparam ReturnType The return type of the function.
     * @tparam Args The types of the arguments to pass to the function.
     * @param alias The alias of the script containing the function.
     * @param function_name The name of the function to call.
     * @param args The arguments to pass to the function.
     * @return The result of the function call.
     */
    template <typename ReturnType, typename... Args>
    ReturnType call_function(const std::string& alias,
                             const std::string& function_name, Args... args);

    /**
     * @brief Gets a variable from a loaded Python script.
     * @tparam T The type of the variable.
     * @param alias The alias of the script containing the variable.
     * @param variable_name The name of the variable to get.
     * @return The value of the variable.
     */
    template <typename T>
    T get_variable(const std::string& alias, const std::string& variable_name);

    /**
     * @brief Sets a variable in a loaded Python script.
     * @param alias The alias of the script containing the variable.
     * @param variable_name The name of the variable to set.
     * @param value The value to set the variable to.
     */
    void set_variable(const std::string& alias,
                      const std::string& variable_name,
                      const py::object& value);

    /**
     * @brief Gets a list of functions in a loaded Python script.
     * @param alias The alias of the script.
     * @return A vector of function names.
     */
    std::vector<std::string> get_function_list(const std::string& alias);

    /**
     * @brief Calls a method of a class in a loaded Python script.
     * @param alias The alias of the script containing the class.
     * @param class_name The name of the class.
     * @param method_name The name of the method to call.
     * @param args The arguments to pass to the method.
     * @return The result of the method call.
     */
    py::object call_method(const std::string& alias,
                           const std::string& class_name,
                           const std::string& method_name,
                           const py::args& args);

    /**
     * @brief Gets an attribute of an object in a loaded Python script.
     * @tparam T The type of the attribute.
     * @param alias The alias of the script containing the object.
     * @param class_name The name of the class of the object.
     * @param attr_name The name of the attribute to get.
     * @return The value of the attribute.
     */
    template <typename T>
    T get_object_attribute(const std::string& alias,
                           const std::string& class_name,
                           const std::string& attr_name);

    /**
     * @brief Sets an attribute of an object in a loaded Python script.
     * @param alias The alias of the script containing the object.
     * @param class_name The name of the class of the object.
     * @param attr_name The name of the attribute to set.
     * @param value The value to set the attribute to.
     */
    void set_object_attribute(const std::string& alias,
                              const std::string& class_name,
                              const std::string& attr_name,
                              const py::object& value);

    /**
     * @brief Evaluates an expression in a loaded Python script.
     * @param alias The alias of the script.
     * @param expression The expression to evaluate.
     * @return The result of the evaluation.
     */
    py::object eval_expression(const std::string& alias,
                               const std::string& expression);

    /**
     * @brief Calls a function in a loaded Python script that returns a list.
     * @param alias The alias of the script containing the function.
     * @param function_name The name of the function to call.
     * @param input_list The list to pass to the function.
     * @return The list returned by the function.
     */
    std::vector<int> call_function_with_list_return(
        const std::string& alias, const std::string& function_name,
        const std::vector<int>& input_list);

    /**
     * @brief Lists all loaded scripts.
     * @return A vector of script aliases.
     */
    std::vector<std::string> list_scripts() const;

    /**
     * @brief Adds a path to the Python sys.path list.
     * @param path The path to add.
     */
    void add_sys_path(const std::string& path);

    /**
     * @brief Synchronizes a variable to Python global namespace.
     * @param name The name of the variable.
     * @param value The value to synchronize.
     */
    void sync_variable_to_python(const std::string& name, py::object value);

    /**
     * @brief Synchronizes a variable from Python global namespace.
     * @param name The name of the variable.
     * @return The synchronized value.
     */
    py::object sync_variable_from_python(const std::string& name);

    /**
     * @brief Executes multiple Python scripts in parallel threads.
     * @param scripts The scripts to execute.
     */
    void execute_script_multithreaded(const std::vector<std::string>& scripts);

    /**
     * @brief Executes a Python script with performance profiling.
     * @param script_content The content of the script to execute.
     */
    void execute_with_profiling(const std::string& script_content);

    /**
     * @brief Injects code into the Python runtime.
     * @param code_snippet The code to inject.
     */
    void inject_code(const std::string& code_snippet);

    /**
     * @brief Registers a C++ function to be callable from Python.
     * @param name The name to register the function under.
     * @param func The function to register.
     */
    void register_function(const std::string& name, std::function<void()> func);

    /**
     * @brief Gets Python memory usage information.
     * @return Python object containing memory usage data.
     */
    py::object get_memory_usage();

    /**
     * @brief Handles Python exceptions.
     * @param e The exception to handle.
     */
    static void handle_exception(const py::error_already_set& e);

    /**
     * @brief Executes a Python script with logging to a file.
     * @param script_content The content of the script to execute.
     * @param log_file The file to log output to.
     */
    void execute_script_with_logging(const std::string& script_content,
                                     const std::string& log_file);

    /**
     * @brief Error handling strategy options
     */
    enum class ErrorHandlingStrategy {
        THROW_EXCEPTION,  ///< Throw exceptions on errors
        RETURN_DEFAULT,   ///< Return default values on errors
        LOG_AND_CONTINUE  ///< Log errors and continue execution
    };

    /**
     * @brief Sets the error handling strategy.
     * @param strategy The strategy to use.
     */
    void set_error_handling_strategy(ErrorHandlingStrategy strategy);

    /**
     * @brief Configuration structure for performance tuning.
     */
    struct PerformanceConfig {
        bool enable_threading;         ///< Whether to use threading
        bool enable_gil_optimization;  ///< Whether to optimize GIL usage
        size_t thread_pool_size;       ///< Size of thread pool
        bool enable_caching;           ///< Whether to use caching
    };

    /**
     * @brief Configures performance settings.
     * @param config The configuration to use.
     */
    void configure_performance(const PerformanceConfig& config);

    /**
     * @brief Asynchronously calls a Python function.
     * @tparam ReturnType The return type of the function.
     * @param alias The alias of the script containing the function.
     * @param function_name The name of the function to call.
     * @return A future that will contain the result.
     */
    template <typename ReturnType>
    std::future<ReturnType> async_call_function(
        const std::string& alias, const std::string& function_name);

    /**
     * @brief Executes multiple Python functions in batch mode.
     * @tparam ReturnType The return type of the functions.
     * @param alias The alias of the script containing the functions.
     * @param function_names The names of the functions to call.
     * @return A vector containing the results.
     */
    template <typename ReturnType>
    std::vector<ReturnType> batch_execute(
        const std::string& alias,
        const std::vector<std::string>& function_names);

    /**
     * @brief Manages the lifecycle of a Python object.
     * @param alias The alias of the script containing the object.
     * @param object_name The name of the object.
     * @param auto_cleanup Whether to automatically clean up the object.
     */
    void manage_object_lifecycle(const std::string& alias,
                                 const std::string& object_name,
                                 bool auto_cleanup = true);

    /**
     * @brief Converts a C++ value to a Python object.
     * @tparam T The type of the value.
     * @param value The value to convert.
     * @return The converted Python object.
     */
    template <typename T>
    py::object cpp_to_python(const T& value);

    /**
     * @brief Converts a Python object to a C++ value.
     * @tparam T The type to convert to.
     * @param obj The Python object to convert.
     * @return The converted C++ value.
     */
    template <typename T>
    T python_to_cpp(const py::object& obj);

    /**
     * @brief Optimizes memory usage.
     */
    void optimize_memory_usage();

    /**
     * @brief Clears unused resources.
     */
    void clear_unused_resources();

    /**
     * @brief Enables or disables debug mode.
     * @param enable Whether to enable debug mode.
     */
    void enable_debug_mode(bool enable = true);

    /**
     * @brief Sets a breakpoint in a Python script.
     * @param alias The alias of the script.
     * @param line_number The line number to set the breakpoint at.
     */
    void set_breakpoint(const std::string& alias, int line_number);

    /**
     * @brief Installs a Python package.
     * @param package_name The name of the package to install.
     * @return Whether the installation was successful.
     */
    bool install_package(const std::string& package_name);

    /**
     * @brief Uninstalls a Python package.
     * @param package_name The name of the package to uninstall.
     * @return Whether the uninstallation was successful.
     */
    bool uninstall_package(const std::string& package_name);

    /**
     * @brief Creates a Python virtual environment.
     * @param env_name The name of the environment to create.
     */
    void create_virtual_environment(const std::string& env_name);

    /**
     * @brief Activates a Python virtual environment.
     * @param env_name The name of the environment to activate.
     */
    void activate_virtual_environment(const std::string& env_name);

    /**
     * @brief Coroutine generator type for asynchronous operations.
     * @tparam T The type yielded by the generator.
     */
    template <typename T>
    struct AsyncGenerator {
        struct promise_type {
            T value;
            AsyncGenerator get_return_object() {
                return AsyncGenerator{handle_type::from_promise(*this)};
            }
            std::suspend_always initial_suspend() { return {}; }
            std::suspend_always final_suspend() noexcept { return {}; }
            void return_void() {}
            std::suspend_always yield_value(T v) {
                value = v;
                return {};
            }
            void unhandled_exception() { throw; }
        };

        using handle_type = std::coroutine_handle<promise_type>;
        handle_type handle;

        AsyncGenerator(handle_type h) : handle(h) {}
        ~AsyncGenerator() {
            if (handle)
                handle.destroy();
        }

        /**
         * @brief Gets the current value of the generator.
         * @return The current value.
         */
        T current_value() { return handle.promise().value; }

        /**
         * @brief Advances the generator to the next value.
         * @return Whether there is another value.
         */
        bool move_next() {
            handle.resume();
            return !handle.done();
        }
    };

    /**
     * @brief Processes a range of elements using a Python function.
     * @tparam R The range type.
     * @param alias The alias of the script containing the function.
     * @param function_name The name of the function to call.
     * @param range The range of elements to process.
     */
    template <std::ranges::range R>
        requires PythonConvertible<std::ranges::range_value_t<R>>
    void batch_process(const std::string& alias,
                       const std::string& function_name, R&& range);

    /**
     * @brief Processes data using a Python function.
     * @tparam T The type of the data elements.
     * @param alias The alias of the script containing the function.
     * @param function_name The name of the function to call.
     * @param data The data to process.
     */
    template <typename T>
    void process_data(const std::string& alias,
                      const std::string& function_name, std::span<T> data);

    /**
     * @brief Gets a shared resource.
     * @tparam T The type of the resource.
     * @param resource_name The name of the resource.
     * @return The shared resource.
     */
    template <typename T>
    std::shared_ptr<T> get_shared_resource(const std::string& resource_name);

    /**
     * @brief Executes a Python script asynchronously.
     * @param script The script to execute.
     * @return An async generator that yields the results.
     */
    AsyncGenerator<py::object> async_execute(const std::string& script);

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;

    // Concurrency control
    mutable std::shared_mutex resource_mutex_;
    std::counting_semaphore<> task_semaphore_{10};  // Limits concurrent tasks
    std::barrier<> sync_point_{2};                  // For synchronization
    std::latch completion_latch_{1};                // For waiting completion
};

/**
 * @brief Custom deleter for Python objects.
 */
struct PythonObjectDeleter {
    void operator()(py::object* obj) {
        py::gil_scoped_acquire gil;
        delete obj;
    }
};

/**
 * @brief RAII wrapper for the Python GIL.
 */
class GILManager {
public:
    /**
     * @brief Constructs a GILManager and acquires the GIL.
     */
    GILManager() : gil_state_(PyGILState_Ensure()) {}

    /**
     * @brief Destroys the GILManager and releases the GIL.
     */
    ~GILManager() { PyGILState_Release(gil_state_); }

private:
    PyGILState_STATE gil_state_;
};

}  // namespace lithium

#endif  // LITHIUM_SCRIPT_PYTHON_WRAPPER_HPP
