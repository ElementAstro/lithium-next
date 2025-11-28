# Lithium Components Test Suite

本目录包含 Lithium 组件系统的完整测试套件，与 `src/components` 目录结构一一对应。

## 目录结构

```
tests/components/
├── dependency/              # 依赖图测试
│   ├── CMakeLists.txt
│   └── test_dependency_graph.cpp
├── loader/                  # 模块加载器测试
│   ├── CMakeLists.txt
│   └── test_module_loader.cpp
├── manager/                 # 组件管理器测试
│   ├── CMakeLists.txt
│   └── test_component_manager.cpp
├── tracker/                 # 文件追踪器测试
│   ├── CMakeLists.txt
│   └── test_file_tracker.cpp
├── version/                 # 版本管理测试
│   ├── CMakeLists.txt
│   └── test_version.cpp
├── system/                  # 系统依赖管理测试
│   ├── CMakeLists.txt
│   ├── test_dependency_manager.cpp
│   ├── test_platform_detector.cpp
│   └── test_package_manager_registry.cpp
├── CMakeLists.txt          # 顶层测试配置
└── README.md               # 本文件
```

## 测试覆盖

### Dependency (依赖图)
- ✅ 节点添加/删除
- ✅ 依赖关系管理
- ✅ 循环依赖检测
- ✅ 拓扑排序
- ✅ 版本冲突检测
- ✅ 并行依赖解析
- ✅ 分组管理
- ✅ 缓存操作
- ✅ package.json 解析
- ✅ 线程安全

### Loader (模块加载器)
- ✅ 模块加载/卸载
- ✅ 模块启用/禁用
- ✅ 函数获取
- ✅ 实例创建
- ✅ 依赖验证
- ✅ 模块重载
- ✅ 状态管理
- ✅ 异步加载
- ✅ 优先级设置
- ✅ 批处理操作

### Manager (组件管理器)
- ✅ 组件初始化/销毁
- ✅ 组件加载/卸载
- ✅ 组件扫描
- ✅ 依赖图更新
- ✅ 事件通知
- ✅ 批量加载
- ✅ 性能监控
- ✅ 配置更新
- ✅ 错误处理
- ✅ 文件监听

### Tracker (文件追踪器)
- ✅ 目录扫描
- ✅ 文件比较
- ✅ 差异记录
- ✅ 文件恢复
- ✅ 异步操作
- ✅ 加密支持
- ✅ 缓存管理
- ✅ 统计信息
- ✅ 批处理
- ✅ 实时监听

### Version (版本管理)
- ✅ 版本解析
- ✅ 版本比较
- ✅ 版本验证
- ✅ 语义化版本
- ✅ 版本范围
- ✅ 预发布版本

### System (系统依赖管理)
- ✅ 平台检测
  - Windows/macOS/Linux 识别
  - 发行版检测
  - 默认包管理器推断
  - 模拟测试支持
- ✅ 包管理器注册表
  - 系统包管理器加载
  - 配置文件解析
  - 命令生成
  - 依赖搜索
  - 安装取消
- ✅ 依赖管理器
  - 依赖添加/删除
  - 配置导入/导出
  - 版本兼容性检查
  - 依赖图生成
  - 缓存刷新
  - 异步安装

## 构建与运行

### 构建所有测试
```bash
cd build
cmake ..
cmake --build . --target lithium_components_tests
```

### 运行所有测试
```bash
ctest --test-dir build -R lithium_.*_tests
```

### 运行特定组件测试
```bash
# 依赖图测试
ctest --test-dir build -R lithium_dependency_tests

# 加载器测试
ctest --test-dir build -R lithium_loader_tests

# 管理器测试
ctest --test-dir build -R lithium_manager_tests

# 追踪器测试
ctest --test-dir build -R lithium_tracker_tests

# 版本测试
ctest --test-dir build -R lithium_version_tests

# 系统依赖测试
ctest --test-dir build -R lithium_system_dependency_tests
```

### 详细输出
```bash
ctest --test-dir build -R lithium_.*_tests --verbose
```

## 测试策略

### 单元测试
每个组件都有独立的单元测试，覆盖核心功能和边界情况。

### 集成测试
- 组件间交互测试
- 依赖关系验证
- 事件传播测试

### 模拟测试
- 平台检测使用模拟 OS 信息
- 包管理器命令生成测试
- 文件系统操作使用临时目录

### 并发测试
- 线程安全验证
- 并发加载测试
- 竞态条件检测

## 注意事项

1. **环境依赖**
   - 某些测试需要实际的包管理器（如 apt、brew）
   - 使用 `GTEST_SKIP()` 跳过不可用的测试
   - 模拟测试不依赖真实环境

2. **临时文件**
   - 测试会创建临时文件和目录
   - 测试结束后自动清理
   - 失败时可能留下临时文件

3. **平台特定**
   - 某些测试仅在特定平台运行
   - 使用条件编译和运行时检测
   - 跨平台测试使用模拟数据

4. **性能测试**
   - 大规模操作测试可能耗时较长
   - 可通过 `--gtest_filter` 选择性运行
   - CI 环境可能需要调整超时设置

## 贡献指南

### 添加新测试
1. 在对应组件目录创建测试文件
2. 更新该目录的 CMakeLists.txt
3. 确保测试可独立运行
4. 添加必要的文档说明

### 测试命名规范
- 测试类：`<Component>Test`
- 测试用例：`TEST_F(<Component>Test, <Functionality>)`
- 模拟测试：`TEST(<Component>MockTest, <Scenario>)`

### 断言使用
- 使用 `EXPECT_*` 用于非致命断言
- 使用 `ASSERT_*` 用于必须满足的前提条件
- 使用 `GTEST_SKIP()` 跳过不适用的测试

## 持续集成

测试套件集成到 CI/CD 流程：
- 每次提交自动运行
- PR 合并前必须通过
- 覆盖率报告自动生成
- 失败测试自动通知

## 许可证

与主项目相同的许可证。
