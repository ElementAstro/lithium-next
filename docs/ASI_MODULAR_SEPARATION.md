# 🏗️ ASI 模块化架构 - 独立组件设计

## 架构概述

根据您的要求，我已经将ASI相机系统完全重构为**三个独立的专用模块**，每个模块都可以独立运行和部署：

```
src/device/asi/
├── camera/                     # 🎯 纯相机功能模块
│   ├── core/                  # 相机核心控制
│   │   ├── asi_camera_core.hpp
│   │   └── asi_camera_core.cpp
│   ├── exposure/              # 曝光控制组件
│   │   ├── exposure_controller.hpp
│   │   └── exposure_controller.cpp
│   ├── temperature/           # 温度管理组件
│   │   ├── temperature_controller.hpp
│   │   └── temperature_controller.cpp
│   ├── component_base.hpp     # 组件基类
│   └── CMakeLists.txt        # 独立构建配置
│
├── filterwheel/               # 🎯 独立滤镜轮模块
│   ├── asi_filterwheel.hpp   # EFW专用控制器
│   ├── asi_filterwheel.cpp   # 完整EFW实现
│   └── CMakeLists.txt        # 独立构建配置
│
└── focuser/                   # 🎯 独立对焦器模块
    ├── asi_focuser.hpp       # EAF专用控制器
    ├── asi_focuser.cpp       # 完整EAF实现
    └── CMakeLists.txt        # 独立构建配置
```

## 🎯 模块分离的核心优势

### 1. **完全独立运行**

```cpp
// 只使用相机，不需要配件
auto camera = createASICameraCore("ASI294MC Pro");
camera->connect("ASI294MC Pro");
camera->startExposure(30.0);

// 只使用滤镜轮，不需要相机
auto filterwheel = createASIFilterWheel("ASI EFW");
filterwheel->connect("EFW #0");
filterwheel->setFilterPosition(2);

// 只使用对焦器，不需要其他设备
auto focuser = createASIFocuser("ASI EAF");
focuser->connect("EAF #0");
focuser->setPosition(15000);
```

### 2. **独立的SDK依赖**

```cmake
# 相机模块 - 只需要ASI Camera SDK
find_library(ASI_CAMERA_LIBRARY NAMES ASICamera2)

# 滤镜轮模块 - 只需要EFW SDK
find_library(ASI_EFW_LIBRARY NAMES EFW_filter)

# 对焦器模块 - 只需要EAF SDK
find_library(ASI_EAF_LIBRARY NAMES EAF_focuser)
```

### 3. **灵活的部署选项**

```bash
# 构建所有模块
cmake --build build --target asi_camera_core asi_filterwheel asi_focuser

# 只构建需要的模块
cmake --build build --target asi_camera_core     # 只要相机
cmake --build build --target asi_filterwheel     # 只要滤镜轮
cmake --build build --target asi_focuser         # 只要对焦器
```

## 🔧 模块功能详解

### ASI 相机模块 (`src/device/asi/camera/`)

**专注纯相机功能**：

- ✅ 图像捕获和曝光控制
- ✅ 相机参数设置（增益、偏移、ROI等）
- ✅ 内置冷却系统控制
- ✅ USB流量优化
- ✅ 图像格式和位深度管理
- ✅ 实时统计和监控

```cpp
auto camera = createASICameraCore("ASI294MC Pro");
camera->initialize();
camera->connect("ASI294MC Pro");

// 相机设置
camera->setGain(300);
camera->setOffset(50);
camera->setBinning(2, 2);
camera->setROI(100, 100, 800, 600);

// 冷却控制
camera->enableCooling(true);
camera->setCoolingTarget(-15.0);

// 曝光控制
camera->startExposure(60.0);
while (camera->isExposing()) {
    double progress = camera->getExposureProgress();
    std::cout << "Progress: " << (progress * 100) << "%" << std::endl;
}

auto frame = camera->getImageData();
```

### ASI 滤镜轮模块 (`src/device/asi/filterwheel/`)

**专门的EFW控制**：

- ✅ 5/7/8位置滤镜轮支持
- ✅ 自定义滤镜命名
- ✅ 单向/双向运动模式
- ✅ 滤镜偏移补偿
- ✅ 序列自动化
- ✅ 配置保存/加载

```cpp
auto efw = createASIFilterWheel("ASI EFW");
efw->initialize();
efw->connect("EFW #0");

// 滤镜配置
std::vector<std::string> filters = {"L", "R", "G", "B", "Ha", "OIII", "SII"};
efw->setFilterNames(filters);
efw->enableUnidirectionalMode(true);

// 滤镜切换
efw->setFilterPosition(2);  // 切换到R滤镜
while (efw->isMoving()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

// 滤镜序列
std::vector<int> sequence = {1, 2, 3, 4};  // L, R, G, B
efw->startFilterSequence(sequence, 1.0);   // 1秒延迟

// 偏移补偿
efw->setFilterOffset(2, 0.25);  // R滤镜偏移0.25
```

### ASI 对焦器模块 (`src/device/asi/focuser/`)

**专业的EAF控制**：

- ✅ 精确位置控制（0-31000步）
- ✅ 温度监控和补偿
- ✅ 反冲补偿
- ✅ 自动对焦算法
- ✅ 位置预设管理
- ✅ V曲线对焦

```cpp
auto eaf = createASIFocuser("ASI EAF");
eaf->initialize();
eaf->connect("EAF #0");

// 基本位置控制
eaf->setPosition(15000);
while (eaf->isMoving()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

// 反冲补偿
eaf->enableBacklashCompensation(true);
eaf->setBacklashSteps(50);

// 温度补偿
eaf->enableTemperatureCompensation(true);
eaf->setTemperatureCoefficient(-2.0);  // 每度-2步

// 自动对焦
auto bestPos = eaf->performCoarseFineAutofocus(500, 50, 2000);
if (bestPos) {
    std::cout << "Best focus at: " << *bestPos << std::endl;
}

// 预设位置
eaf->savePreset("near", 10000);
eaf->savePreset("infinity", 25000);
auto pos = eaf->loadPreset("near");
if (pos) eaf->setPosition(*pos);
```

## 🚀 协调使用示例

虽然模块是独立的，但可以协调使用：

```cpp
// 创建独立的设备实例
auto camera = createASICameraCore("ASI294MC Pro");
auto filterwheel = createASIFilterWheel("ASI EFW");
auto focuser = createASIFocuser("ASI EAF");

// 独立初始化和连接
camera->initialize() && camera->connect("ASI294MC Pro");
filterwheel->initialize() && filterwheel->connect("EFW #0");
focuser->initialize() && focuser->connect("EAF #0");

// 协调拍摄序列
std::vector<std::string> filters = {"L", "R", "G", "B"};
std::vector<int> filterPositions = {1, 2, 3, 4};

for (size_t i = 0; i < filters.size(); ++i) {
    // 切换滤镜
    filterwheel->setFilterPosition(filterPositions[i]);
    while (filterwheel->isMoving()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // 对焦（如果需要）
    if (i == 0) {  // 只在第一个滤镜时对焦
        auto bestFocus = focuser->performCoarseFineAutofocus(200, 20, 1000);
        if (bestFocus) focuser->setPosition(*bestFocus);
    }

    // 拍摄
    camera->startExposure(120.0);
    while (camera->isExposing()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    auto frame = camera->getImageData();
    // 保存图像...
}
```

## 📦 构建和安装

### 独立构建每个模块

```bash
# 只构建相机模块
cd src/device/asi/camera
cmake -B build -S .
cmake --build build

# 只构建滤镜轮模块
cd src/device/asi/filterwheel
cmake -B build -S .
cmake --build build

# 只构建对焦器模块
cd src/device/asi/focuser
cmake -B build -S .
cmake --build build
```

### 全局构建配置

```cmake
# 主CMakeLists.txt中的选项
option(BUILD_ASI_CAMERA "Build ASI Camera module" ON)
option(BUILD_ASI_FILTERWHEEL "Build ASI Filter Wheel module" ON)
option(BUILD_ASI_FOCUSER "Build ASI Focuser module" ON)

if(BUILD_ASI_CAMERA)
    add_subdirectory(src/device/asi/camera)
endif()

if(BUILD_ASI_FILTERWHEEL)
    add_subdirectory(src/device/asi/filterwheel)
endif()

if(BUILD_ASI_FOCUSER)
    add_subdirectory(src/device/asi/focuser)
endif()
```

### 选择性构建

```bash
# 只构建相机和对焦器，不构建滤镜轮
cmake -B build -S . \
    -DBUILD_ASI_CAMERA=ON \
    -DBUILD_ASI_FILTERWHEEL=OFF \
    -DBUILD_ASI_FOCUSER=ON

cmake --build build
```

## 🎁 分离架构的优势总结

### ✅ **开发优势**

- **独立开发**：每个模块可以独立开发和测试
- **减少依赖**：不需要安装不用的SDK
- **编译速度**：只编译需要的模块
- **调试简化**：问题隔离在特定模块

### ✅ **部署优势**

- **灵活部署**：根据硬件配置选择模块
- **资源优化**：只加载需要的功能
- **更新独立**：可以单独更新某个模块
- **故障隔离**：某个模块故障不影响其他模块

### ✅ **用户优势**

- **按需使用**：只使用拥有的硬件
- **学习简化**：专注于需要的功能
- **配置清晰**：每个设备独立配置
- **扩展性好**：容易添加新的ASI设备

### ✅ **系统优势**

- **内存效率**：不加载未使用的功能
- **启动速度**：减少初始化时间
- **稳定性好**：模块间错误不传播
- **维护简单**：清晰的模块边界

这种分离架构使Lithium成为真正模块化的天体摄影平台，用户可以根据自己的硬件配置和需求灵活选择和使用相应的模块。
