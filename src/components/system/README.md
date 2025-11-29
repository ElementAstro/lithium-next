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

## API 示例

### 检查依赖是否已安装

```cpp
#include "components/system/dependency_system.hpp"

using namespace lithium::system;

DependencyManager manager("./config/package_managers.json");

// 检查包是否已安装
bool installed = manager.isDependencyInstalled("cmake");

// 获取已安装版本
auto version = manager.getInstalledVersion("cmake");
if (version) {
    std::cout << "cmake v" << version->major << "."
              << version->minor << "." << version->patch << std::endl;
}
```

### 安装依赖

```cpp
// 添加托管依赖
DependencyInfo dep;
dep.name = "cmake";
dep.version = {3, 20, 0, ""};
dep.packageManager = "apt";  // 或 "choco", "brew" 等
manager.addDependency(dep);

// 异步安装
auto future = manager.install("cmake");
auto result = future.get();
if (result.value) {
    std::cout << "已安装: " << *result.value << std::endl;
}
```

### 版本兼容性检查

```cpp
auto result = manager.checkVersionCompatibility("cmake", "3.10.0");
if (result.value && *result.value) {
    std::cout << "版本兼容" << std::endl;
}
```

## 支持的包管理器

| 平台 | 包管理器 |
|------|---------|
| Linux | apt, dnf, yum, pacman, zypper, flatpak, snap |
| macOS | brew, port |
| Windows | choco, scoop, winget |

## 配置文件

包管理器可通过 JSON 配置：

```json
{
    "package_managers": [
        {
            "name": "apt",
            "check_cmd": "dpkg -l {}",
            "install_cmd": "sudo apt-get install -y {}",
            "uninstall_cmd": "sudo apt-get remove -y {}",
            "search_cmd": "apt-cache search {}"
        }
    ]
}
```

配置文件搜索路径（按优先级依次尝试，可同时加载多个存在的配置）：
- `./config/package_managers.json`
- `./package_managers.json`
- `../config/package_managers.json`
- `~/.lithium/package_managers.json`
- `/etc/lithium/package_managers.json`
- (Windows) `%APPDATA%\lithium\package_managers.json`
- (Windows) `%PROGRAMDATA%\lithium\package_managers.json`

> 提示：可以通过 `DependencyManager manager("/path/to/custom.json");` 传入额外配置，或将文件放在上述任一路径中，系统会自动合并已找到的配置。
