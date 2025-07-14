# 系统依赖管理组件的CMakeLists.txt
cmake_minimum_required(VERSION 3.20)

# 添加系统依赖管理组件库
add_library(lithium_system_dependency STATIC
    dependency_types.cpp
    dependency_manager.cpp
    platform_detector.cpp
    package_manager.cpp
)

# 设置包含目录
target_include_directories(lithium_system_dependency PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

# 链接必要的库
target_link_libraries(lithium_system_dependency
    # 这里需要链接原项目中的atom库和其他依赖
    # atom::system
    # atom::async
    # atom
    # nlohmann_json
)

# 可选：创建测试可执行文件
if(BUILD_TESTING)
    add_executable(test_system_dependency test_example.cpp)
    target_link_libraries(test_system_dependency lithium_system_dependency)
endif()

# 设置编译标准
target_compile_features(lithium_system_dependency PUBLIC cxx_std_20)
