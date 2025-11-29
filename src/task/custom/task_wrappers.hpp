/**
 * @file task_wrappers.hpp
 * @brief Task wrapper classes for factory registration
 */

#ifndef LITHIUM_TASK_WRAPPERS_HPP
#define LITHIUM_TASK_WRAPPERS_HPP

#include "../task.hpp"
#include "atom/type/json.hpp"

namespace lithium::task {

using json = nlohmann::json;

// Device Tasks
class DeviceConnectTask : public Task {
public:
    using Task::Task;
    DeviceConnectTask(const std::string& name, const json& config)
        : Task(name, [this](const json& p) { executeImpl(p); }),
          config_(config) {}
    static auto taskName() -> std::string { return "DeviceConnect"; }
    void execute(const json& params) override { executeImpl(params); }

private:
    json config_;
    void executeImpl(const json& params) {
        std::string deviceName = params.value("device_name", "");
        addHistoryEntry("Connecting: " + deviceName);
    }
};

class DeviceDisconnectTask : public Task {
public:
    using Task::Task;
    DeviceDisconnectTask(const std::string& name, const json& config)
        : Task(name, [this](const json& p) { executeImpl(p); }),
          config_(config) {}
    static auto taskName() -> std::string { return "DeviceDisconnect"; }
    void execute(const json& params) override { executeImpl(params); }

private:
    json config_;
    void executeImpl(const json& params) {
        std::string deviceName = params.value("device_name", "");
        addHistoryEntry("Disconnecting: " + deviceName);
    }
};

// Config Tasks
class LoadConfigTask : public Task {
public:
    using Task::Task;
    LoadConfigTask(const std::string& name, const json& config)
        : Task(name, [this](const json& p) { executeImpl(p); }),
          config_(config) {}
    static auto taskName() -> std::string { return "LoadConfig"; }
    void execute(const json& params) override { executeImpl(params); }

private:
    json config_;
    void executeImpl(const json& params) {
        std::string path = params.value("config_path", "");
        addHistoryEntry("Loading config: " + path);
    }
};

class SaveConfigTask : public Task {
public:
    using Task::Task;
    SaveConfigTask(const std::string& name, const json& config)
        : Task(name, [this](const json& p) { executeImpl(p); }),
          config_(config) {}
    static auto taskName() -> std::string { return "SaveConfig"; }
    void execute(const json& params) override { executeImpl(params); }

private:
    json config_;
    void executeImpl(const json& params) {
        std::string path = params.value("config_path", "");
        addHistoryEntry("Saving config: " + path);
    }
};

// Script Tasks
class RunScriptTask : public Task {
public:
    using Task::Task;
    RunScriptTask(const std::string& name, const json& config)
        : Task(name, [this](const json& p) { executeImpl(p); }),
          config_(config) {}
    static auto taskName() -> std::string { return "RunScript"; }
    void execute(const json& params) override { executeImpl(params); }

private:
    json config_;
    void executeImpl(const json& params) {
        std::string path = params.value("script_path", "");
        addHistoryEntry("Running script: " + path);
    }
};

class RunWorkflowTask : public Task {
public:
    using Task::Task;
    RunWorkflowTask(const std::string& name, const json& config)
        : Task(name, [this](const json& p) { executeImpl(p); }),
          config_(config) {}
    static auto taskName() -> std::string { return "RunWorkflow"; }
    void execute(const json& params) override { executeImpl(params); }

private:
    json config_;
    void executeImpl(const json& params) {
        std::string name = params.value("workflow_name", "");
        addHistoryEntry("Running workflow: " + name);
    }
};

// Search Tasks
class TargetSearchTask : public Task {
public:
    using Task::Task;
    TargetSearchTask(const std::string& name, const json& config)
        : Task(name, [this](const json& p) { executeImpl(p); }),
          config_(config) {}
    static auto taskName() -> std::string { return "TargetSearch"; }
    void execute(const json& params) override { executeImpl(params); }
    const json& getLastResults() const { return lastResults_; }

private:
    json config_;
    json lastResults_;
    void executeImpl(const json& params) {
        std::string target = params.value("target_name", "");
        addHistoryEntry("Searching: " + target);
        lastResults_ = json::object();
    }
};

// Mount Tasks
class MountSlewTask : public Task {
public:
    using Task::Task;
    MountSlewTask(const std::string& name, const json& config)
        : Task(name, [this](const json& p) { executeImpl(p); }),
          config_(config) {}
    static auto taskName() -> std::string { return "MountSlew"; }
    void execute(const json& params) override { executeImpl(params); }

private:
    json config_;
    void executeImpl(const json& params) {
        double ra = params.value("ra", 0.0);
        double dec = params.value("dec", 0.0);
        addHistoryEntry("Slewing to RA=" + std::to_string(ra) +
                        ", Dec=" + std::to_string(dec));
    }
};

class MountParkTask : public Task {
public:
    using Task::Task;
    MountParkTask(const std::string& name, const json& config)
        : Task(name, [this](const json& p) { executeImpl(p); }),
          config_(config) {}
    static auto taskName() -> std::string { return "MountPark"; }
    void execute(const json& params) override { executeImpl(params); }

private:
    json config_;
    void executeImpl(const json& params) {
        std::string pos = params.value("park_position", "default");
        addHistoryEntry("Parking mount: " + pos);
    }
};

class MountTrackTask : public Task {
public:
    using Task::Task;
    MountTrackTask(const std::string& name, const json& config)
        : Task(name, [this](const json& p) { executeImpl(p); }),
          config_(config) {}
    static auto taskName() -> std::string { return "MountTrack"; }
    void execute(const json& params) override { executeImpl(params); }

private:
    json config_;
    void executeImpl(const json& params) {
        bool enabled = params.value("enabled", true);
        addHistoryEntry("Tracking " +
                        std::string(enabled ? "enabled" : "disabled"));
    }
};

// Focuser Tasks
class FocuserMoveTask : public Task {
public:
    using Task::Task;
    FocuserMoveTask(const std::string& name, const json& config)
        : Task(name, [this](const json& p) { executeImpl(p); }),
          config_(config) {}
    static auto taskName() -> std::string { return "FocuserMove"; }
    void execute(const json& params) override { executeImpl(params); }

private:
    json config_;
    void executeImpl(const json& params) {
        int pos = params.value("position", 0);
        addHistoryEntry("Moving focuser to " + std::to_string(pos));
    }
};

}  // namespace lithium::task

#endif  // LITHIUM_TASK_WRAPPERS_HPP
