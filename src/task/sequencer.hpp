#ifndef LITHIUM_TASK_SEQUENCER_HPP
#define LITHIUM_TASK_SEQUENCER_HPP

#include <atomic>
#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include "../database/orm.hpp"
#include "generator.hpp"
#include "target.hpp"

namespace lithium::sequencer {
using namespace lithium::database;
// 枚举表示序列的状态
enum class SequenceState { Idle, Running, Paused, Stopping, Stopped };

// 假设 TargetStatus 已在 target.hpp 中定义
// enum class TargetStatus { Pending, Running, Completed, Failed, Skipped };

class SequenceModel {
public:
    static std::string tableName() { return "sequences"; }

    static std::vector<std::unique_ptr<ColumnBase>> columns() {
        std::vector<std::unique_ptr<ColumnBase>> cols;
        cols.push_back(std::make_unique<Column<std::string, SequenceModel>>(
            "uuid", &SequenceModel::uuid));
        cols.push_back(std::make_unique<Column<std::string, SequenceModel>>(
            "name", &SequenceModel::name));
        cols.push_back(std::make_unique<Column<std::string, SequenceModel>>(
            "data", &SequenceModel::data));
        cols.push_back(std::make_unique<Column<std::string, SequenceModel>>(
            "created_at", &SequenceModel::createdAt));
        return cols;
    }

    std::string uuid;
    std::string name;
    std::string data;
    std::string createdAt;
};

class ExposureSequence {
public:
    // 回调函数类型定义
    using SequenceCallback = std::function<void()>;
    using TargetCallback =
        std::function<void(const std::string& targetName, TargetStatus status)>;
    using ErrorCallback = std::function<void(const std::string& targetName,
                                             const std::exception& e)>;

    ExposureSequence();
    ~ExposureSequence();

    // 禁止拷贝
    ExposureSequence(const ExposureSequence&) = delete;
    ExposureSequence& operator=(const ExposureSequence&) = delete;

    // 目标管理
    void addTarget(std::unique_ptr<Target> target);
    void removeTarget(const std::string& name);
    void modifyTarget(const std::string& name, const TargetModifier& modifier);

    // 执行控制
    void executeAll();
    void stop();
    void pause();
    void resume();

    // 序列化
    void saveSequence(const std::string& filename) const;
    void loadSequence(const std::string& filename);

    // 查询函数
    std::vector<std::string> getTargetNames() const;
    TargetStatus getTargetStatus(const std::string& name) const;
    double getProgress() const;  // 返回进度百分比

    // 回调设置函数
    void setOnSequenceStart(SequenceCallback callback);
    void setOnSequenceEnd(SequenceCallback callback);
    void setOnTargetStart(TargetCallback callback);
    void setOnTargetEnd(TargetCallback callback);
    void setOnError(ErrorCallback callback);
    void handleTargetError(Target* target, const std::exception& e);

    // 调度策略枚举
    enum class SchedulingStrategy {
        FIFO,         // 先进先出
        Priority,     // 优先级调度
        Dependencies  // 依赖关系调度
    };

    // 错误恢复策略枚举
    enum class RecoveryStrategy {
        Stop,        // 停止序列
        Skip,        // 跳过失败的任务
        Retry,       // 重试失败的任务
        Alternative  // 执行替代任务
    };

    // 新增方法
    void setSchedulingStrategy(SchedulingStrategy strategy);
    void setRecoveryStrategy(RecoveryStrategy strategy);
    void addAlternativeTarget(const std::string& targetName,
                              std::unique_ptr<Target> alternative);

    // 性能监控
    [[nodiscard]] auto getAverageExecutionTime() const
        -> std::chrono::milliseconds;
    [[nodiscard]] auto getTotalMemoryUsage() const -> size_t;

    // 依赖管理
    void addTargetDependency(const std::string& targetName,
                             const std::string& dependsOn);
    void removeTargetDependency(const std::string& targetName,
                                const std::string& dependsOn);
    void setTargetPriority(const std::string& targetName, int priority);
    [[nodiscard]] auto isTargetReady(const std::string& targetName) const
        -> bool;
    [[nodiscard]] auto getTargetDependencies(
        const std::string& targetName) const -> std::vector<std::string>;

    // 新增的监控和控制功能
    void setMaxConcurrentTargets(size_t max);
    void setTargetTimeout(const std::string& name,
                          std::chrono::seconds timeout);
    void setGlobalTimeout(std::chrono::seconds timeout);

    // 状态查询
    [[nodiscard]] auto getFailedTargets() const -> std::vector<std::string>;
    [[nodiscard]] auto getExecutionStats() const -> json;
    [[nodiscard]] auto getResourceUsage() const -> json;

    // 错误恢复
    void retryFailedTargets();
    void skipFailedTargets();

    /**
     * @brief Sets parameters for a specific task in a target
     * @param targetName The name of the target
     * @param taskUUID The UUID of the task
     * @param params The parameters in JSON format
     */
    void setTargetTaskParams(const std::string& targetName,
                             const std::string& taskUUID, const json& params);

    /**
     * @brief Gets parameters for a specific task in a target
     * @param targetName The name of the target
     * @param taskUUID The UUID of the task
     * @return The task parameters if found
     */
    [[nodiscard]] auto getTargetTaskParams(const std::string& targetName,
                                           const std::string& taskUUID) const
        -> std::optional<json>;

    /**
     * @brief Sets parameters for a target
     * @param targetName The name of the target
     * @param params The parameters in JSON format
     */
    void setTargetParams(const std::string& targetName, const json& params);

    /**
     * @brief Gets parameters for a target
     * @param targetName The name of the target
     * @return The target parameters if found
     */
    [[nodiscard]] auto getTargetParams(const std::string& targetName) const
        -> std::optional<json>;

    /**
     * @brief Save the current sequence to the database.
     */
    void saveToDatabase();

    /**
     * @brief Load a sequence from the database using a unique identifier.
     * @param uuid The unique identifier of the sequence to load.
     */
    void loadFromDatabase(const std::string& uuid);

    /**
     * @brief List all sequences stored in the database.
     * @return A vector of SequenceModel representing all sequences.
     */
    std::vector<SequenceModel> listSequences();

    /**
     * @brief Delete a sequence from the database using a unique identifier.
     * @param uuid The unique identifier of the sequence to delete.
     */
    void deleteFromDatabase(const std::string& uuid);

    /**
     * @brief Set the task generator.
     * @param generator A shared pointer to the TaskGenerator to set.
     */
    void setTaskGenerator(std::shared_ptr<TaskGenerator> generator);

    /**
     * @brief Get the current task generator.
     * @return A shared pointer to the current TaskGenerator.
     */
    auto getTaskGenerator() const -> std::shared_ptr<TaskGenerator>;

    /**
     * @brief Process a specific target with macros.
     * @param targetName The name of the target to process.
     */
    void processTargetWithMacros(const std::string& targetName);

    /**
     * @brief Process all targets with macros.
     */
    void processAllTargetsWithMacros();

    /**
     * @brief Add a macro to the sequence.
     * @param name The name of the macro.
     * @param value The value of the macro.
     */
    void addMacro(const std::string& name, MacroValue value);

    /**
     * @brief Remove a macro from the sequence.
     * @param name The name of the macro to remove.
     */
    void removeMacro(const std::string& name);

    /**
     * @brief List all macros in the sequence.
     * @return A vector of strings representing the names of all macros.
     */
    auto listMacros() const -> std::vector<std::string>;

private:
    std::vector<std::unique_ptr<Target>> targets_;
    mutable std::shared_mutex mutex_;
    std::atomic<SequenceState> state_{SequenceState::Idle};
    std::jthread sequenceThread_;

    // 进度跟踪
    std::atomic<size_t> completedTargets_{0};
    size_t totalTargets_ = 0;

    // 回调函数
    SequenceCallback onSequenceStart_;
    SequenceCallback onSequenceEnd_;
    TargetCallback onTargetStart_;
    TargetCallback onTargetEnd_;
    ErrorCallback onError_;

    SchedulingStrategy schedulingStrategy_{SchedulingStrategy::FIFO};
    RecoveryStrategy recoveryStrategy_{RecoveryStrategy::Stop};
    std::map<std::string, std::unique_ptr<Target>> alternativeTargets_;

    // 辅助方法
    void executeSequence();
    void notifySequenceStart();
    void notifySequenceEnd();
    void notifyTargetStart(const std::string& name);
    void notifyTargetEnd(const std::string& name, TargetStatus status);
    void notifyError(const std::string& name, const std::exception& e);

    // 调度辅助方法
    void reorderTargetsByDependencies();
    [[nodiscard]] auto getNextExecutableTarget() -> Target*;

    void updateTargetReadyStatus();
    auto findCyclicDependency() const -> std::optional<std::string>;
    void reorderTargetsByPriority();

    std::unordered_map<std::string, std::vector<std::string>>
        targetDependencies_;
    std::unordered_map<std::string, bool> targetReadyStatus_;

    size_t maxConcurrentTargets_{1};
    std::chrono::seconds globalTimeout_{0};
    std::atomic<size_t> failedTargets_{0};
    std::vector<std::string> failedTargetNames_;

    // 新增的监控数据
    struct ExecutionStats {
        std::chrono::steady_clock::time_point startTime;
        size_t totalExecutions{0};
        size_t successfulExecutions{0};
        size_t failedExecutions{0};
        double averageExecutionTime{0.0};
    } stats_;

    std::string uuid_;  // 序列的唯一标识符
    std::shared_ptr<database::Database> db_;
    std::unique_ptr<database::Table<SequenceModel>> sequenceTable_;

    // 序列化和反序列化助手方法
    json serializeToJson() const;
    void deserializeFromJson(const json& data);

    std::shared_ptr<TaskGenerator> taskGenerator_;

    // 辅助方法
    void initializeDefaultMacros();
    void processJsonWithGenerator(json& data);
};

}  // namespace lithium::sequencer

#endif  // LITHIUM_TASK_SEQUENCER_HPP
