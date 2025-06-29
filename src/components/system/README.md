# 系统依赖管理组件

本目录包含了系统依赖管理的拆分组件，用于更好的代码组织和维护。

## 文件结构

```
system/
├── dependency_exception.hpp     # 异常和错误类型定义
├── dependency_types.hpp         # 基础数据类型定义
├── dependency_types.cpp         # 数据类型实现
├── dependency_manager.hpp       # 主要依赖管理器接口
├── dependency_manager.cpp       # 依赖管理器实现
├── platform_detector.hpp        # 平台检测器接口
├── platform_detector.cpp        # 平台检测器实现
├── package_manager.hpp          # 包管理器注册表接口
├── package_manager.cpp          # 包管理器注册表实现
├── lru_cache.hpp                # LRU缓存模板类
├── dependency_system.hpp        # 统一包含头文件
└── README.md                    # 本文件
```

## 组件说明

### 核心组件

1. **DependencyManager** - 主要的依赖管理器类，提供所有对外接口
2. **PlatformDetector** - 平台和发行版检测
3. **PackageManagerRegistry** - 包管理器注册和管理

### 数据类型

1. **DependencyInfo** - 依赖项信息
2. **VersionInfo** - 版本信息
3. **PackageManagerInfo** - 包管理器信息
4. **DependencyResult** - 异步操作结果

### 异常处理

1. **DependencyException** - 依赖管理异常
2. **DependencyError** - 结构化错误信息
3. **DependencyErrorCode** - 错误代码枚举

### 工具类

1. **LRUCache** - 通用LRU缓存实现

## 向后兼容性

根目录下的 `system_dependency.hpp` 和 `system_dependency.cpp` 提供了向后兼容的包装，
使用类型别名将原来的 `lithium` 命名空间映射到新的 `lithium::system` 命名空间。

## 使用方法

### 新代码
```cpp
#include "system/dependency_system.hpp"
using namespace lithium::system;

DependencyManager manager;
manager.addDependency({"cmake", {3, 20, 0}, "apt"});
```

### 兼容旧代码
```cpp
#include "system_dependency.hpp"
using namespace lithium;

DependencyManager manager;  // 自动映射到 lithium::system::DependencyManager
```

## 编译说明

所有源文件都应该被包含在编译过程中：
- dependency_types.cpp
- dependency_manager.cpp
- platform_detector.cpp
- package_manager.cpp
- system_dependency.cpp (向后兼容包装)
