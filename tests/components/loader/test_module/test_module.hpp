#ifndef TEST_MODULE_HPP
#define TEST_MODULE_HPP

#include <memory>
#include <string>
#include "atom/type/json_fwd.hpp"

using json = nlohmann::json;

namespace test_module {

// Simple test class
class TestClass {
public:
    TestClass() = default;
    explicit TestClass(const json& config);
    ~TestClass() = default;

    std::string getName() const;
    int getValue() const;
    void setValue(int value);

private:
    std::string name_{"default"};
    int value_{0};
};

// Export function declarations
extern "C" {
// Simple function exports
int add(int a, int b);
int multiply(int a, int b);
const char* getVersion();

// Instance creation functions
std::shared_ptr<TestClass> createInstance(const json& config);
std::unique_ptr<TestClass> createUniqueInstance(const json& config);
}

}  // namespace test_module

#endif  // TEST_MODULE_HPP
