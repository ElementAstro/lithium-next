/**
 * @file test_component_exceptions.cpp
 * @brief Unit tests for component manager exceptions
 *
 * Tests the exceptions defined in components/manager/exceptions.hpp
 */

#include <gtest/gtest.h>
#include <stdexcept>
#include <string>

#include "components/manager/exceptions.hpp"

namespace lithium::test {

// ============================================================================
// FailToLoadComponent Exception Tests
// ============================================================================

class FailToLoadComponentTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(FailToLoadComponentTest, ThrowAndCatch) {
    bool caught = false;

    try {
        throw FailToLoadComponent("test_file.cpp", 42, "testFunction",
                                  "Failed to load component");
    } catch (const FailToLoadComponent& ex) {
        caught = true;
        // Exception should contain the message
        std::string what = ex.what();
        EXPECT_FALSE(what.empty());
    }

    EXPECT_TRUE(caught);
}

TEST_F(FailToLoadComponentTest, CatchAsAtomException) {
    bool caught = false;

    try {
        throw FailToLoadComponent("test.cpp", 10, "func", "Error message");
    } catch (const atom::error::Exception& ex) {
        caught = true;
        EXPECT_NE(std::string(ex.what()).length(), 0);
    }

    EXPECT_TRUE(caught);
}

TEST_F(FailToLoadComponentTest, CatchAsStdException) {
    bool caught = false;

    try {
        throw FailToLoadComponent("file.cpp", 1, "function", "Some error");
    } catch (const std::exception& ex) {
        caught = true;
        EXPECT_NE(std::string(ex.what()).length(), 0);
    }

    EXPECT_TRUE(caught);
}

TEST_F(FailToLoadComponentTest, ExceptionMessage) {
    try {
        throw FailToLoadComponent("component.cpp", 100, "loadComponent",
                                  "Component not found");
    } catch (const FailToLoadComponent& ex) {
        std::string what = ex.what();
        // The message should contain some information
        EXPECT_FALSE(what.empty());
    }
}

TEST_F(FailToLoadComponentTest, DifferentMessages) {
    std::vector<std::string> messages = {
        "Component not found",
        "Invalid configuration",
        "Dependency missing",
        "Version mismatch",
        "Permission denied"
    };

    for (const auto& msg : messages) {
        bool caught = false;
        try {
            throw FailToLoadComponent("test.cpp", 1, "test", msg);
        } catch (const FailToLoadComponent& ex) {
            caught = true;
            EXPECT_FALSE(std::string(ex.what()).empty());
        }
        EXPECT_TRUE(caught);
    }
}

TEST_F(FailToLoadComponentTest, EmptyMessage) {
    bool caught = false;

    try {
        throw FailToLoadComponent("test.cpp", 1, "func", "");
    } catch (const FailToLoadComponent& ex) {
        caught = true;
        // Even with empty message, what() should return something
    }

    EXPECT_TRUE(caught);
}

TEST_F(FailToLoadComponentTest, LongMessage) {
    bool caught = false;
    std::string longMessage(10000, 'x');

    try {
        throw FailToLoadComponent("test.cpp", 1, "func", longMessage);
    } catch (const FailToLoadComponent& ex) {
        caught = true;
        EXPECT_FALSE(std::string(ex.what()).empty());
    }

    EXPECT_TRUE(caught);
}

TEST_F(FailToLoadComponentTest, SpecialCharactersInMessage) {
    bool caught = false;

    try {
        throw FailToLoadComponent("test.cpp", 1, "func",
                                  "Error with special chars: <>&\"'\n\t");
    } catch (const FailToLoadComponent& ex) {
        caught = true;
        EXPECT_FALSE(std::string(ex.what()).empty());
    }

    EXPECT_TRUE(caught);
}

TEST_F(FailToLoadComponentTest, UnicodeMessage) {
    bool caught = false;

    try {
        throw FailToLoadComponent("test.cpp", 1, "func",
                                  "Unicode error: 中文消息 日本語");
    } catch (const FailToLoadComponent& ex) {
        caught = true;
        EXPECT_FALSE(std::string(ex.what()).empty());
    }

    EXPECT_TRUE(caught);
}

// ============================================================================
// Exception Propagation Tests
// ============================================================================

class ExceptionPropagationTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}

    void throwException() {
        throw FailToLoadComponent("inner.cpp", 50, "innerFunc",
                                  "Inner exception");
    }

    void wrapException() {
        try {
            throwException();
        } catch (...) {
            throw;  // Re-throw
        }
    }
};

TEST_F(ExceptionPropagationTest, RethrowException) {
    bool caught = false;

    try {
        wrapException();
    } catch (const FailToLoadComponent& ex) {
        caught = true;
        EXPECT_FALSE(std::string(ex.what()).empty());
    }

    EXPECT_TRUE(caught);
}

TEST_F(ExceptionPropagationTest, ExceptionInFunction) {
    auto throwingFunc = []() {
        throw FailToLoadComponent("lambda.cpp", 1, "lambda",
                                  "Exception from lambda");
    };

    bool caught = false;
    try {
        throwingFunc();
    } catch (const FailToLoadComponent& ex) {
        caught = true;
    }

    EXPECT_TRUE(caught);
}

TEST_F(ExceptionPropagationTest, NestedTryCatch) {
    bool innerCaught = false;
    bool outerCaught = false;

    try {
        try {
            throw FailToLoadComponent("test.cpp", 1, "func", "Inner");
        } catch (const FailToLoadComponent&) {
            innerCaught = true;
            throw FailToLoadComponent("test.cpp", 2, "func", "Outer");
        }
    } catch (const FailToLoadComponent&) {
        outerCaught = true;
    }

    EXPECT_TRUE(innerCaught);
    EXPECT_TRUE(outerCaught);
}

// ============================================================================
// THROW_FAIL_TO_LOAD_COMPONENT Macro Tests
// ============================================================================

class ThrowMacroTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ThrowMacroTest, MacroThrowsException) {
    bool caught = false;

    try {
        THROW_FAIL_TO_LOAD_COMPONENT("Test error from macro");
    } catch (const FailToLoadComponent& ex) {
        caught = true;
        EXPECT_FALSE(std::string(ex.what()).empty());
    }

    EXPECT_TRUE(caught);
}

TEST_F(ThrowMacroTest, MacroWithDifferentMessages) {
    std::vector<std::string> messages = {
        "Error 1",
        "Error 2",
        "Error 3"
    };

    for (const auto& msg : messages) {
        bool caught = false;
        try {
            THROW_FAIL_TO_LOAD_COMPONENT(msg);
        } catch (const FailToLoadComponent& ex) {
            caught = true;
        }
        EXPECT_TRUE(caught);
    }
}

// ============================================================================
// Exception Safety Tests
// ============================================================================

class ExceptionSafetyTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ExceptionSafetyTest, ExceptionDoesNotLeak) {
    // Test that throwing and catching doesn't cause memory leaks
    for (int i = 0; i < 1000; ++i) {
        try {
            throw FailToLoadComponent("test.cpp", i, "func",
                                      "Iteration " + std::to_string(i));
        } catch (const FailToLoadComponent&) {
            // Caught and handled
        }
    }

    // If we get here without crashing, the test passes
    EXPECT_TRUE(true);
}

TEST_F(ExceptionSafetyTest, MultipleExceptionTypes) {
    // Test catching different exception types
    int catchCount = 0;

    try {
        throw FailToLoadComponent("test.cpp", 1, "func", "Error");
    } catch (const FailToLoadComponent&) {
        catchCount++;
    }

    try {
        throw std::runtime_error("Standard error");
    } catch (const std::runtime_error&) {
        catchCount++;
    }

    EXPECT_EQ(catchCount, 2);
}

}  // namespace lithium::test
