# Lithium 任务系统使用指南

## 概述

Lithium 项目提供了一个强大的任务执行系统，包含以下核心组件：

- **ExposureSequence**: 任务序列管理器，负责执行和调度多个Target
- **Target**: 目标对象，包含一个或多个Task
- **Task**: 具体的任务实现，支持各种操作

## 基本使用方法

### 1. 创建和配置序列管理器

```cpp
#include "task/sequencer.hpp"
#include "task/target.hpp"

using namespace lithium::sequencer;

// 创建序列管理器
ExposureSequence sequence;

// 设置回调函数
sequence.setOnSequenceStart([]() {
    std::cout << "序列开始执行" << std::endl;
});

sequence.setOnSequenceEnd([]() {
    std::cout << "序列执行完成" << std::endl;
});

sequence.setOnTargetStart([](const std::string& name, TargetStatus status) {
    std::cout << "目标 " << name << " 开始执行" << std::endl;
});

sequence.setOnTargetEnd([](const std::string& name, TargetStatus status) {
    std::cout << "目标 " << name << " 执行完成，状态：" << static_cast<int>(status) << std::endl;
});

sequence.setOnError([](const std::string& name, const std::exception& e) {
    std::cerr << "目标 " << name << " 执行错误：" << e.what() << std::endl;
});
```

### 2. 创建自定义任务

```cpp
// 方法1：直接创建Task
auto customTask = std::make_unique<Task>("CustomTask", [](const json& params) {
    std::cout << "执行自定义任务，参数：" << params.dump() << std::endl;

    // 获取参数
    double exposure = params.value("exposure", 1.0);
    int gain = params.value("gain", 100);

    // 执行具体操作
    std::this_thread::sleep_for(std::chrono::seconds(2));

    std::cout << "任务完成，曝光时间：" << exposure << "s，增益：" << gain << std::endl;
});

// 方法2：使用预定义的任务类
DeviceManager deviceManager;
auto deviceTask = std::make_unique<DeviceTask>("MyDeviceTask", deviceManager);

auto configTask = std::make_unique<task::TaskConfigManagement>("ConfigTask");
```

### 3. 创建目标并添加任务

```cpp
// 创建目标（名称，冷却时间，最大重试次数）
auto target = std::make_unique<Target>("MainTarget", std::chrono::seconds(5), 3);

// 添加任务到目标
target->addTask(std::move(customTask));
target->addTask(std::move(deviceTask));
target->addTask(std::move(configTask));

// 配置目标
target->setEnabled(true);
target->setMaxRetries(2);
```

### 4. 添加目标到序列并执行

```cpp
// 添加目标到序列
sequence.addTarget(std::move(target));

// 设置目标参数
sequence.setTargetParams("MainTarget", {
    {"exposure", 2.5},
    {"gain", 150},
    {"mode", "auto"}
});

// 配置执行策略
sequence.setSchedulingStrategy(ExposureSequence::SchedulingStrategy::FIFO);
sequence.setRecoveryStrategy(ExposureSequence::RecoveryStrategy::Retry);
sequence.setMaxConcurrentTargets(2);

// 执行序列
sequence.executeAll();

// 或者在线程中执行
std::thread execThread([&sequence]() {
    sequence.executeAll();
});
execThread.join();
```

## 预定义任务类型

### 1. DeviceTask - 设备管理任务

```cpp
#include "task/custom/device_task.hpp"

DeviceManager deviceManager;
auto deviceTask = std::make_unique<DeviceTask>("DeviceControl", deviceManager);

// 使用参数执行设备操作
json deviceParams = {
    {"operation", "connect"},
    {"deviceName", "camera1"},
    {"timeout", 5000}
};

deviceTask->execute(deviceParams);

// 或者直接调用方法
deviceTask->connectDevice("camera1", 5000);
deviceTask->scanDevices("camera");
deviceTask->initializeDevice("camera1");
```

支持的设备操作：
- `connect`: 连接设备
- `scan`: 扫描设备
- `initialize`: 初始化设备
- `disconnect`: 断开设备
- `reset`: 重置设备

### 2. ConfigTask - 配置管理任务

```cpp
#include "task/custom/config_task.hpp"

auto configTask = std::make_unique<task::TaskConfigManagement>("ConfigManager");

// 设置配置
json setParams = {
    {"operation", "set"},
    {"key_path", "camera.exposure"},
    {"value", 2.5}
};
configTask->execute(setParams);

// 获取配置
json getParams = {
    {"operation", "get"},
    {"key_path", "camera.exposure"}
};
configTask->execute(getParams);

// 保存配置
json saveParams = {
    {"operation", "save"},
    {"file_path", "/path/to/config.json"}
};
configTask->execute(saveParams);
```

支持的配置操作：
- `set`: 设置配置值
- `get`: 获取配置值
- `delete`: 删除配置
- `load`: 从文件加载配置
- `save`: 保存配置到文件
- `merge`: 合并配置
- `list`: 列出配置

### 3. ScriptTask - 脚本执行任务

```cpp
#include "task/custom/script_task.hpp"

auto scriptTask = std::make_unique<task::ScriptTask>(
    "ScriptRunner",
    "/path/to/script_config.json",
    "/path/to/analyzer_config.json"
);

json scriptParams = {
    {"operation", "execute"},
    {"script_name", "calibration_script"},
    {"script_path", "/scripts/calibration.py"},
    {"args", {"--mode", "auto", "--output", "/tmp/result"}}
};

scriptTask->execute(scriptParams);
```

## 高级功能

### 1. 任务依赖关系

```cpp
// 添加目标依赖
sequence.addTargetDependency("Target3", "Target1");  // Target3依赖Target1
sequence.addTargetDependency("Target3", "Target2");  // Target3依赖Target2

// 添加任务依赖
task1->addDependency("task0_uuid");
```

### 2. 任务参数定义和验证

```cpp
// 自定义任务时添加参数定义
auto task = std::make_unique<Task>("ValidatedTask", [](const json& params) {
    // 任务逻辑
});

task->addParamDefinition("exposure", "number", true, 1.0, "曝光时间(秒)");
task->addParamDefinition("gain", "number", false, 100, "相机增益");
task->addParamDefinition("mode", "string", false, "auto", "拍摄模式");

// 参数验证会在执行前自动进行
```

### 3. 错误处理和重试

```cpp
// 设置异常处理回调
task->setExceptionCallback([](const std::exception& e) {
    std::cerr << "任务异常：" << e.what() << std::endl;
});

// 设置恢复策略
sequence.setRecoveryStrategy(ExposureSequence::RecoveryStrategy::Retry);  // 重试
// 或者
sequence.setRecoveryStrategy(ExposureSequence::RecoveryStrategy::Skip);   // 跳过
sequence.setRecoveryStrategy(ExposureSequence::RecoveryStrategy::Stop);   // 停止
```

### 4. 任务监控和状态查询

```cpp
// 获取执行进度
double progress = sequence.getProgress();

// 获取目标状态
TargetStatus status = sequence.getTargetStatus("Target1");

// 获取失败的目标
auto failedTargets = sequence.getFailedTargets();

// 获取执行统计
auto stats = sequence.getExecutionStats();
```

### 5. 动态任务管理

```cpp
// 在执行过程中动态添加目标
auto newTarget = std::make_unique<Target>("DynamicTarget");
auto newTask = std::make_unique<Task>("DynamicTask", [](const json& params) {
    std::cout << "动态添加的任务" << std::endl;
});
newTarget->addTask(std::move(newTask));
sequence.addTarget(std::move(newTarget));

// 修改现有目标
sequence.modifyTarget("Target1", [](Target& target) {
    target.setMaxRetries(5);
    target.setCooldown(std::chrono::seconds(10));
});
```

### 6. 序列保存和加载

```cpp
// 保存序列到文件
sequence.saveSequence("/path/to/sequence.json");

// 从文件加载序列
sequence.loadSequence("/path/to/sequence.json");
```

## 完整示例

```cpp
#include <iostream>
#include <thread>
#include "task/sequencer.hpp"
#include "task/target.hpp"
#include "task/custom/device_task.hpp"
#include "task/custom/config_task.hpp"

using namespace lithium::sequencer;

int main() {
    // 1. 创建序列管理器
    ExposureSequence sequence;

    // 2. 设置回调
    sequence.setOnSequenceStart([]() {
        std::cout << "=== 开始执行任务序列 ===" << std::endl;
    });

    sequence.setOnTargetEnd([](const std::string& name, TargetStatus status) {
        std::cout << "目标 " << name << " 完成，状态：" << static_cast<int>(status) << std::endl;
    });

    // 3. 创建目标和任务
    auto target1 = std::make_unique<Target>("InitTarget", std::chrono::seconds(2), 2);

    // 配置管理任务
    auto configTask = std::make_unique<task::TaskConfigManagement>("Config");
    target1->addTask(std::move(configTask));

    // 设备管理任务
    DeviceManager deviceManager;
    auto deviceTask = std::make_unique<DeviceTask>("Device", deviceManager);
    target1->addTask(std::move(deviceTask));

    // 自定义任务
    auto customTask = std::make_unique<Task>("Custom", [](const json& params) {
        std::cout << "执行自定义任务：" << params.dump() << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    });
    target1->addTask(std::move(customTask));

    // 4. 配置序列
    sequence.addTarget(std::move(target1));
    sequence.setTargetParams("InitTarget", {
        {"operation", "initialize"},
        {"deviceName", "camera1"},
        {"config_key", "system.ready"},
        {"custom_param", "test_value"}
    });

    // 5. 执行序列
    std::thread execThread([&sequence]() {
        sequence.executeAll();
    });

    // 6. 监控进度
    while (sequence.getProgress() < 100.0) {
        std::cout << "进度：" << sequence.getProgress() << "%" << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    execThread.join();

    // 7. 获取结果
    auto stats = sequence.getExecutionStats();
    std::cout << "执行统计：" << stats.dump(2) << std::endl;

    return 0;
}
```

## 最佳实践

1. **任务设计原则**：
   - 每个任务应该有明确的单一职责
   - 添加适当的参数验证和错误处理
   - 使用有意义的任务名称和描述

2. **错误处理**：
   - 设置适当的重试策略
   - 使用异常回调记录错误信息
   - 考虑任务失败对整个序列的影响

3. **性能优化**：
   - 合理设置并发限制
   - 使用任务依赖优化执行顺序
   - 避免不必要的任务等待

4. **监控和调试**：
   - 使用日志记录任务执行过程
   - 定期检查任务状态和进度
   - 保存执行统计用于分析

这个任务系统非常灵活强大，你可以根据具体需求来组合和配置不同的任务类型。
