# 基本设置
cmake_minimum_required(VERSION 3.5)
project(ssbindings)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找依赖包
find_package(pybind11 REQUIRED)
find_package(Qt6 COMPONENTS Core REQUIRED)
find_package(StellarSolver REQUIRED)

# 源文件定义
set(SOURCE_FILES
    binding.cpp
    statistic.cpp
    stellarsolver.cpp
)

# 创建 PyBind11 模块
pybind11_add_module(ssbindings MODULE ${SOURCE_FILES})

# 设置目标属性和包含目录
target_include_directories(ssbindings PRIVATE
    ${pybind11_INCLUDE_DIRS}
    ${Qt6Core_INCLUDE_DIRS}
    ${STELLARSOLVER_INCLUDE_DIRS}
)

# 链接依赖库
target_link_libraries(ssbindings PRIVATE
    Qt6::Core
    stellarsolver
)

# 启用 Qt MOC
set_target_properties(ssbindings PROPERTIES
    AUTOMOC ON
)