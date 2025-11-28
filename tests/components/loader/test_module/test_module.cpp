#include "test_module.hpp"
#include "atom/type/json.hpp"

namespace test_module {

TestClass::TestClass(const json& config) {
    if (config.contains("name")) {
        name_ = config["name"].get<std::string>();
    }
    if (config.contains("value")) {
        value_ = config["value"].get<int>();
    }
}

std::string TestClass::getName() const {
    return name_;
}

int TestClass::getValue() const {
    return value_;
}

void TestClass::setValue(int value) {
    value_ = value;
}

extern "C" {

int add(int a, int b) {
    return a + b;
}

int multiply(int a, int b) {
    return a * b;
}

const char* getVersion() {
    return "1.0.0";
}

std::shared_ptr<TestClass> createInstance(const json& config) {
    return std::make_shared<TestClass>(config);
}

std::unique_ptr<TestClass> createUniqueInstance(const json& config) {
    return std::make_unique<TestClass>(config);
}

} // extern "C"

} // namespace test_module
