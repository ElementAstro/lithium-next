#include <iostream>
#include <thread>
#include "task/core/sequencer.hpp"
#include "task/core/target.hpp"

using namespace lithium::task;

int main() {
    // Create an instance of ExposureSequence
    ExposureSequence sequence;

    // Set callback functions
    sequence.setOnSequenceStart(
        []() { std::cout << "Sequence started." << std::endl; });

    sequence.setOnSequenceEnd(
        []() { std::cout << "Sequence ended." << std::endl; });

    sequence.setOnTargetStart(
        [](const std::string& targetName, TargetStatus status) {
            std::cout << "Target " << targetName << " started, status "
                      << static_cast<int>(status) << "." << std::endl;
        });

    sequence.setOnTargetEnd(
        [](const std::string& targetName, TargetStatus status) {
            std::cout << "Target " << targetName << " ended, status "
                      << static_cast<int>(status) << "." << std::endl;
        });

    sequence.setOnError(
        [](const std::string& targetName, const std::exception& e) {
            std::cerr << "Target " << targetName << " error: " << e.what()
                      << std::endl;
        });

    // Create and add targets with their tasks
    auto target1 =
        std::make_unique<Target>("Target1", std::chrono::seconds(5), 3);
    auto task1 = std::make_unique<Task>("Task1", [](const json& params) {
        std::cout << "Task1 executing, exposure time: " << params["exposure"]
                  << "s, gain: " << params["gain"] << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(2));
    });
    target1->addTask(std::move(task1));

    auto target2 =
        std::make_unique<Target>("Target2", std::chrono::seconds(3), 2);
    auto task2 = std::make_unique<Task>("Task2", [](const json& params) {
        std::cout << "Task2 executing, mode: " << params["mode"] << std::endl;
        // Simulate failure
        THROW_RUNTIME_ERROR("Simulated Task2 failure");
    });
    target2->addTask(std::move(task2));

    auto target3 =
        std::make_unique<Target>("Target3", std::chrono::seconds(4), 1);
    auto task3 = std::make_unique<Task>("Task3", [](const json& params) {
        std::cout << "Task3 executing, path: " << params["path"] << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    });
    target3->addTask(std::move(task3));

    // Add targets to the sequence
    sequence.addTarget(std::move(target1));
    sequence.addTarget(std::move(target2));
    sequence.addTarget(std::move(target3));

    // Set target parameters
    sequence.setTargetParams("Target1", {{"exposure", 1.5}, {"gain", 150}});
    sequence.setTargetParams("Target2", {{"mode", "auto"}});
    sequence.setTargetParams("Target3", {{"path", "/data/output/"}});

    // Set scheduling and recovery strategies
    sequence.setSchedulingStrategy(ExposureSequence::SchedulingStrategy::FIFO);
    sequence.setRecoveryStrategy(ExposureSequence::RecoveryStrategy::Retry);

    // Add target dependencies
    sequence.addTargetDependency("Target3", "Target1");
    sequence.addTargetDependency("Target3", "Target2");

    // Set maximum concurrent targets
    sequence.setMaxConcurrentTargets(2);

    // Set global timeout
    sequence.setGlobalTimeout(std::chrono::seconds(600));

    // Execute all targets
    std::thread execThread([&sequence]() { sequence.executeAll(); });

    // Simulate dynamically adding new targets during execution
    std::this_thread::sleep_for(std::chrono::seconds(10));
    auto target4 =
        std::make_unique<Target>("Target4", std::chrono::seconds(2), 2);
    auto task4 = std::make_unique<Task>("Task4", [](const json& params) {
        std::cout << "Task4 executing, params: " << params.dump() << std::endl;
    });
    target4->addTask(std::move(task4));
    sequence.addTarget(std::move(target4));
    sequence.setTargetParams("Target4", {{"param1", "value1"}, {"param2", 42}});
    sequence.addTargetDependency("Target4", "Target3");

    // Wait for execution to complete
    execThread.join();

    // Get execution statistics
    auto stats = sequence.getExecutionStats();
    std::cout << "Execution statistics: " << stats.dump(4) << std::endl;

    // Get failed targets
    auto failedTargets = sequence.getFailedTargets();
    for (const auto& targetName : failedTargets) {
        std::cout << "Failed target: " << targetName << std::endl;
    }

    // Retry failed targets
    if (!failedTargets.empty()) {
        std::cout << "Retrying failed targets..." << std::endl;
        sequence.retryFailedTargets();
    }

    return 0;
}
