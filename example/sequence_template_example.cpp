#include <iostream>
#include <string>
#include <memory>
#include "../src/task/sequencer.hpp"
#include "../src/task/target.hpp"
#include "../src/task/task.hpp"

using namespace lithium::task;
using json = nlohmann::json;

int main() {
    try {
        // Create a sequence
        auto sequence = std::make_unique<ExposureSequence>();

        // Create a target
        auto target = std::make_unique<Target>("M42");

        // Create some generic tasks for the target
        auto task1 = std::make_unique<Task>(
            "Light Frame", [](const json& params) {
                std::cout << "Executing light frame with params: " << params.dump() << std::endl;
            });
        task1->setTaskType("GenericTask");

        auto task2 = std::make_unique<Task>(
            "Flat Frame", [](const json& params) {
                std::cout << "Executing flat frame with params: " << params.dump() << std::endl;
            });
        task2->setTaskType("GenericTask");

        // Add tasks to the target
        target->addTask(std::move(task1));
        target->addTask(std::move(task2));

        // Add the target to the sequence
        sequence->addTarget(std::move(target));

        // Export the sequence as a template
        std::cout << "Exporting sequence as template..." << std::endl;
        sequence->exportAsTemplate("m42_template.json");
        std::cout << "Template exported successfully." << std::endl;

        // Create a new sequence from the template with custom parameters
        json params;
        params["target_name"] = "M51";
        params["exposure_time"] = 60.0;
        params["count"] = 10;

        auto newSequence = std::make_unique<ExposureSequence>();
        std::cout << "Creating sequence from template..." << std::endl;
        newSequence->createFromTemplate("m42_template.json", params);
        std::cout << "Sequence created from template successfully." << std::endl;

        // Save the new sequence to a file
        newSequence->saveSequence("m51_sequence.json");
        std::cout << "Sequence saved to m51_sequence.json" << std::endl;

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
