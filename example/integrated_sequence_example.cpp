/**
 * @file sequence_integration_example.cpp
 * @brief Example demonstrating the integrated task sequence system
 *
 * This example shows how to use the SequenceManager to create, load,
 * and execute task sequences with proper error handling.
 *
 * @date 2025-07-11
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2025 Max Qian
 */

#include <iostream>
#include <chrono>
#include <thread>

#include "task/sequence_manager.hpp"
#include "task/sequencer.hpp"
#include "task/task.hpp"
#include "task/target.hpp"
#include "task/generator.hpp"
#include "task/custom/factory.hpp"
#include "task/registration.hpp"

#include "spdlog/spdlog.h"

using namespace lithium::task;
using json = nlohmann::json;

// Helper function to create a simple target with tasks
std::unique_ptr<Target> createSimpleTarget(const std::string& name, int exposureCount) {
    // Create a target with 5 second cooldown and 2 retries
    auto target = std::make_unique<Target>(name, std::chrono::seconds(5), 2);

    // Create a task that simulates taking an exposure
    for (int i = 0; i < exposureCount; ++i) {
        auto exposureTask = std::make_unique<Task>(
            "Exposure" + std::to_string(i + 1),
            "TakeExposure",
            [i](const json& params) {
                spdlog::info("Taking exposure {} with parameters: {}", i + 1, params.dump());

                // Simulate exposure time
                double exposureTime = params.contains("exposure") ?
                    params["exposure"].get<double>() : 1.0;

                // Use at most 1 second for simulation
                std::this_thread::sleep_for(std::chrono::milliseconds(
                    static_cast<int>(std::min(exposureTime, 1.0) * 1000)));

                spdlog::info("Exposure {} complete", i + 1);
            });

        // Set task priority based on order
        exposureTask->setPriority(i);

        // Add task to target
        target->addTask(std::move(exposureTask));
    }

    // Set target callbacks
    target->setOnStart([name](const std::string&) {
        spdlog::info("Target {} started", name);
    });

    target->setOnEnd([name](const std::string&, TargetStatus status) {
        spdlog::info("Target {} ended with status: {}", name,
                    status == TargetStatus::Completed ? "Completed" :
                    status == TargetStatus::Failed ? "Failed" : "Other");
    });

    target->setOnError([name](const std::string&, const std::exception& e) {
        spdlog::error("Target {} error: {}", name, e.what());
    });

    return target;
}

// Example of creating and saving a sequence
void createAndSaveSequenceExample() {
    try {
        // Initialize sequence manager with default options
        auto manager = SequenceManager::createShared();

        // Create a new sequence
        auto sequence = manager->createSequence("ExampleSequence");

        // Add targets to the sequence
        sequence->addTarget(createSimpleTarget("Target1", 3));
        sequence->addTarget(createSimpleTarget("Target2", 2));

        // Add a dependency between targets
        sequence->addTargetDependency("Target2", "Target1");

        // Set parameters for tasks in targets
        json target1Params = {
            {"exposure", 0.5},
            {"type", "light"},
            {"binning", 1},
            {"gain", 100},
            {"offset", 10}
        };

        json target2Params = {
            {"exposure", 1.0},
            {"type", "dark"},
            {"binning", 2},
            {"gain", 200},
            {"offset", 15}
        };

        sequence->setTargetParams("Target1", target1Params);
        sequence->setTargetParams("Target2", target2Params);

        // Save the sequence to a file
        sequence->saveSequence("example_sequence.json");
        spdlog::info("Sequence saved to example_sequence.json");

        // Save to database for later retrieval
        std::string uuid = manager->saveToDatabase(sequence);
        spdlog::info("Sequence saved to database with UUID: {}", uuid);

    } catch (const SequenceException& e) {
        spdlog::error("Sequence error: {}", e.what());
    } catch (const std::exception& e) {
        spdlog::error("General error: {}", e.what());
    }
}

// Example of loading and executing a sequence
void loadAndExecuteSequenceExample() {
    try {
        // Initialize sequence manager with custom options
        SequenceOptions options;
        options.validateOnLoad = true;
        options.maxConcurrentTargets = 2;
        options.schedulingStrategy = ExposureSequence::SchedulingStrategy::Dependencies;
        options.recoveryStrategy = ExposureSequence::RecoveryStrategy::Retry;

        auto manager = SequenceManager::createShared(options);

        // Register event callbacks
        manager->setOnSequenceStart([](const std::string& id) {
            spdlog::info("Sequence {} started", id);
        });

        manager->setOnSequenceEnd([](const std::string& id, bool success) {
            spdlog::info("Sequence {} ended with status: {}", id, success ? "Success" : "Failure");
        });

        manager->setOnTargetStart([](const std::string& id, const std::string& targetName) {
            spdlog::info("Sequence {}: Target {} started", id, targetName);
        });

        manager->setOnTargetEnd([](const std::string& id, const std::string& targetName, TargetStatus status) {
            spdlog::info("Sequence {}: Target {} ended with status: {}",
                        id, targetName,
                        status == TargetStatus::Completed ? "Completed" :
                        status == TargetStatus::Failed ? "Failed" :
                        status == TargetStatus::Skipped ? "Skipped" : "Other");
        });

        manager->setOnError([](const std::string& id, const std::string& targetName, const std::exception& e) {
            spdlog::error("Sequence {}: Target {} error: {}", id, targetName, e.what());
        });

        // Load sequence from file
        auto sequence = manager->loadSequenceFromFile("example_sequence.json");

        // Execute sequence asynchronously
        auto result = manager->executeSequence(sequence, true);

        // Wait for the sequence to complete or for 30 seconds max
        auto finalResult = manager->waitForCompletion(sequence, std::chrono::seconds(30));

        if (finalResult) {
            spdlog::info("Sequence completed with {} successful targets and {} failed targets",
                        finalResult->completedTargets.size(), finalResult->failedTargets.size());

            spdlog::info("Execution time: {} ms", finalResult->totalExecutionTime.count());
        } else {
            spdlog::warn("Sequence execution timed out or was not found");
        }

    } catch (const SequenceException& e) {
        spdlog::error("Sequence error: {}", e.what());
    } catch (const std::exception& e) {
        spdlog::error("General error: {}", e.what());
    }
}

// Example of creating a sequence from a template
void templateSequenceExample() {
    try {
        // Initialize sequence manager
        auto manager = SequenceManager::createShared();

        // Register built-in templates
        manager->registerBuiltInTaskTemplates();

        // List available templates
        auto templates = manager->listAvailableTemplates();
        spdlog::info("Available templates:");
        for (const auto& templateName : templates) {
            auto info = manager->getTemplateInfo(templateName);
            if (info) {
                spdlog::info("- {} ({}): {}", templateName, info->version, info->description);
            } else {
                spdlog::info("- {}", templateName);
            }
        }

        // Create parameters for the template
        json params = {
            {"targetName", "M42"},
            {"exposureTime", 30.0},
            {"frameType", "light"},
            {"binning", 1},
            {"gain", 100},
            {"offset", 10}
        };

        // Create sequence from template
        auto sequence = manager->createSequenceFromTemplate("BasicExposure", params);

        // Execute sequence synchronously
        auto result = manager->executeSequence(sequence, false);

        if (result) {
            spdlog::info("Template sequence executed with result: {}", result->success ? "Success" : "Failure");
            spdlog::info("Execution time: {} ms", result->totalExecutionTime.count());
        }

    } catch (const SequenceException& e) {
        spdlog::error("Template error: {}", e.what());
    } catch (const std::exception& e) {
        spdlog::error("General error: {}", e.what());
    }
}

// Example of error handling in sequences
void errorHandlingExample() {
    try {
        // Initialize sequence manager with retry strategy
        SequenceOptions options;
        options.recoveryStrategy = ExposureSequence::RecoveryStrategy::Retry;
        options.maxConcurrentTargets = 1;

        auto manager = SequenceManager::createShared(options);

        // Create a sequence
        auto sequence = manager->createSequence("ErrorHandlingSequence");

        // Create a target with an error-prone task
        auto target = std::make_unique<Target>("ErrorTarget", std::chrono::seconds(1), 3);

        // Add a task that will fail on first attempt but succeed on retry
        int attemptCount = 0;
        auto errorTask = std::make_unique<Task>(
            "ErrorTask",
            "ErrorTest",
            [&attemptCount](const json& params) {
                spdlog::info("Executing error-prone task, attempt #{}", ++attemptCount);

                // Fail on first attempt
                if (attemptCount == 1) {
                    spdlog::warn("First attempt failing deliberately");
                    throw std::runtime_error("Deliberate failure on first attempt");
                }

                spdlog::info("Task succeeded on retry");
            });

        // Add task to target
        target->addTask(std::move(errorTask));

        // Add target to sequence
        sequence->addTarget(std::move(target));

        // Execute sequence
        auto result = manager->executeSequence(sequence, false);

        if (result) {
            spdlog::info("Error handling test result: {}", result->success ? "Success" : "Failure");

            if (!result->warnings.empty()) {
                spdlog::info("Warnings:");
                for (const auto& warning : result->warnings) {
                    spdlog::info("- {}", warning);
                }
            }

            if (!result->errors.empty()) {
                spdlog::info("Errors:");
                for (const auto& error : result->errors) {
                    spdlog::info("- {}", error);
                }
            }
        }

    } catch (const SequenceException& e) {
        spdlog::error("Sequence error: {}", e.what());
    } catch (const std::exception& e) {
        spdlog::error("General error: {}", e.what());
    }
}

// Main function demonstrating all examples
int main() {
    // Configure logging
    spdlog::set_level(spdlog::level::info);
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] %v");

    // Register built-in tasks
    registerBuiltInTasks();

    spdlog::info("Starting integrated sequence examples");

    // Run examples
    spdlog::info("\n=== Creating and Saving Sequence Example ===");
    createAndSaveSequenceExample();

    spdlog::info("\n=== Loading and Executing Sequence Example ===");
    loadAndExecuteSequenceExample();

    spdlog::info("\n=== Template Sequence Example ===");
    templateSequenceExample();

    spdlog::info("\n=== Error Handling Example ===");
    errorHandlingExample();

    spdlog::info("\nAll examples completed");

    return 0;
}
