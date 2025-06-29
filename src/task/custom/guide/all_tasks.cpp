#include "all_tasks.hpp"
#include "../factory.hpp"
#include <spdlog/spdlog.h>

namespace lithium::task::guide {

void registerAllGuideTasks() {
    using namespace lithium::task;
    auto& factory = TaskFactory::getInstance();

    // For now, register only the basic connection tasks to ensure compilation works
    // More tasks can be added once the basic structure is working

    // Create TaskInfo for basic tasks
    auto connectInfo = TaskInfo{"GuiderConnect", "Connect to PHD2 guider", "guide", {}, json::object()};
    auto disconnectInfo = TaskInfo{"GuiderDisconnect", "Disconnect from PHD2 guider", "guide", {}, json::object()};

    try {
        // Register basic connection tasks using the factory directly
        REGISTER_TASK_WITH_FACTORY(GuiderConnectTask, "GuiderConnect",
            [](const std::string& name, const json& config) -> std::unique_ptr<GuiderConnectTask> {
                return std::make_unique<GuiderConnectTask>();
            }, connectInfo);

        REGISTER_TASK_WITH_FACTORY(GuiderDisconnectTask, "GuiderDisconnect",
            [](const std::string& name, const json& config) -> std::unique_ptr<GuiderDisconnectTask> {
                return std::make_unique<GuiderDisconnectTask>();
            }, disconnectInfo);

        spdlog::info("Basic guide tasks registered successfully");

    } catch (const std::exception& e) {
        spdlog::error("Failed to register guide tasks: {}", e.what());
        throw;
    }
}

}  // namespace lithium::task::guide

// Register all tasks when the library is loaded
namespace {
struct GuideTaskRegistrar {
    GuideTaskRegistrar() {
        lithium::task::guide::registerAllGuideTasks();
    }
};

static GuideTaskRegistrar g_guide_task_registrar;
}  // namespace
