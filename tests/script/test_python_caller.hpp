#include <gtest/gtest.h>
#include "script/python_caller.hpp"

using namespace lithium;

class PythonWrapperTest : public ::testing::Test {
protected:
    void SetUp() override {
        wrapper = std::make_unique<PythonWrapper>();
        // Create test.py with sample code
        std::ofstream test_script("test.py");
        test_script << R"(
def add(a, b):
    return a + b

def process_list(items):
    return [x * 2 for x in items]

class TestClass:
    def __init__(self):
        self.value = 42

    def get_value(self):
        return self.value

    def set_value(self, val):
        self.value = val

test_var = 100
)";
        test_script.close();
    }

    void TearDown() override {
        wrapper.reset();
        std::remove("test.py");
    }

    std::unique_ptr<PythonWrapper> wrapper;
};

TEST_F(PythonWrapperTest, BasicConstruction) {
    EXPECT_NO_THROW(PythonWrapper());
}

TEST_F(PythonWrapperTest, LoadScript) {
    EXPECT_NO_THROW(wrapper->load_script("test", "test_alias"));
    auto scripts = wrapper->list_scripts();
    EXPECT_TRUE(std::find(scripts.begin(), scripts.end(), "test_alias") !=
                scripts.end());
}

TEST_F(PythonWrapperTest, UnloadScript) {
    wrapper->load_script("test", "test_alias");
    EXPECT_NO_THROW(wrapper->unload_script("test_alias"));
    auto scripts = wrapper->list_scripts();
    EXPECT_TRUE(std::find(scripts.begin(), scripts.end(), "test_alias") ==
                scripts.end());
}

TEST_F(PythonWrapperTest, CallFunction) {
    wrapper->load_script("test", "test_alias");
    int result = wrapper->call_function<int>("test_alias", "add", 5, 3);
    EXPECT_EQ(result, 8);
}

TEST_F(PythonWrapperTest, GetVariable) {
    wrapper->load_script("test", "test_alias");
    int value = wrapper->get_variable<int>("test_alias", "test_var");
    EXPECT_EQ(value, 100);
}

TEST_F(PythonWrapperTest, SetVariable) {
    wrapper->load_script("test", "test_alias");
    py::object new_value = py::cast(200);
    EXPECT_NO_THROW(wrapper->set_variable("test_alias", "test_var", new_value));
    int value = wrapper->get_variable<int>("test_alias", "test_var");
    EXPECT_EQ(value, 200);
}

TEST_F(PythonWrapperTest, CallMethodOnClass) {
    wrapper->load_script("test", "test_alias");
    py::object result = wrapper->call_method("test_alias", "TestClass",
                                             "get_value", py::args());
    EXPECT_EQ(py::cast<int>(result), 42);
}

TEST_F(PythonWrapperTest, ListProcessing) {
    wrapper->load_script("test", "test_alias");
    std::vector<int> input = {1, 2, 3, 4};
    auto result = wrapper->call_function_with_list_return(
        "test_alias", "process_list", input);
    EXPECT_EQ(result, std::vector<int>({2, 4, 6, 8}));
}

TEST_F(PythonWrapperTest, ErrorHandling) {
    wrapper->set_error_handling_strategy(
        PythonWrapper::ErrorHandlingStrategy::THROW_EXCEPTION);
    EXPECT_THROW(wrapper->load_script("nonexistent", "test_alias"),
                 py::error_already_set);
}

TEST_F(PythonWrapperTest, AsyncExecution) {
    wrapper->load_script("test", "test_alias");
    auto future = wrapper->async_call_function<int>("test_alias", "add", 5, 3);
    EXPECT_EQ(future.get(), 8);
}

TEST_F(PythonWrapperTest, PerformanceConfig) {
    PythonWrapper::PerformanceConfig config{.enable_threading = true,
                                            .enable_gil_optimization = true,
                                            .thread_pool_size = 4,
                                            .enable_caching = true};
    EXPECT_NO_THROW(wrapper->configure_performance(config));
}

TEST_F(PythonWrapperTest, MultiThreadedExecution) {
    std::vector<std::string> scripts = {
        "print('Thread 1')", "print('Thread 2')", "print('Thread 3')"};
    EXPECT_NO_THROW(wrapper->execute_script_multithreaded(scripts));
}

TEST_F(PythonWrapperTest, Profiling) {
    testing::internal::CaptureStdout();
    wrapper->execute_with_profiling("print('Test')");
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_TRUE(output.find("Execution time:") != std::string::npos);
}

TEST_F(PythonWrapperTest, CodeInjection) {
    EXPECT_NO_THROW(wrapper->inject_code("x = 42"));
    auto result = wrapper->sync_variable_from_python("x");
    EXPECT_EQ(py::cast<int>(result), 42);
}

TEST_F(PythonWrapperTest, ScriptWithLogging) {
    const char* log_file = "test.log";
    EXPECT_NO_THROW(
        wrapper->execute_script_with_logging("print('Test')", log_file));
    std::ifstream log(log_file);
    EXPECT_TRUE(log.good());
    std::string line;
    std::getline(log, line);
    EXPECT_EQ(line, "Test");
    std::remove(log_file);
}

TEST_F(PythonWrapperTest, MemoryManagement) {
    EXPECT_NO_THROW(wrapper->optimize_memory_usage());
    auto objects = wrapper->get_memory_usage();
    EXPECT_TRUE(py::isinstance<py::list>(objects));
}

TEST_F(PythonWrapperTest, VirtualEnvironment) {
    const char* env_name = "test_env";
    EXPECT_NO_THROW(wrapper->create_virtual_environment(env_name));
    EXPECT_NO_THROW(wrapper->activate_virtual_environment(env_name));
}

TEST_F(PythonWrapperTest, PackageManagement) {
    EXPECT_TRUE(
        wrapper->install_package("six"));  // Using a small, common package
    EXPECT_TRUE(wrapper->uninstall_package("six"));
}

TEST_F(PythonWrapperTest, SysPathManagement) {
    std::string test_path = "/test/path";
    EXPECT_NO_THROW(wrapper->add_sys_path(test_path));
    py::module_ sys = py::module_::import("sys");
    py::list sys_path = sys.attr("path");
    bool path_found = false;
    for (const auto& path : sys_path) {
        if (py::cast<std::string>(path) == test_path) {
            path_found = true;
            break;
        }
    }
    EXPECT_TRUE(path_found);
}
