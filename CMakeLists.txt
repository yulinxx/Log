cmake_minimum_required(VERSION 4.3)

# ============================================================================
# Log —— 独立构建入口
# ============================================================================
# 集成构建：根 CMakeLists.txt → add_subdirectory(Log/Log)（内层）。
# 独立构建：本文件提供 project() 与环境配置，把 Log 单独拉出来开发 / 跑测试。
#
# 三条设计约束同时成立：
#
#   1) 复用仓库统一配置，不自己手搓一套，避免与主工程漂移：
#        ../Config.cmake               —— vcpkg / Qt 路径、MSVC 运行库、模块开关
#        ../CMake/StandaloneInit.cmake —— SANYI_ROOT、编译选项、GTest 查找、兄弟模块拉入
#      与 FileIO / License / Utility / Nesting 的独立入口保持同一套写法。
#
#   2) Log 是叶子模块，只依赖 vcpkg 的 spdlog，没有兄弟 DLL 需要拉入，
#      可脱离主程序单独编译。
#
#   3) 测试由内层 Log/Log/CMakeLists.txt 在 BUILD_LOG_TESTS=ON 时挂载，
#      本文件不得重复 add_subdirectory(Test)，否则 target 重定义。
#
# 用法：
#   cmake -S Log -B Log/build
#   cmake --build Log/build --config Release
#   ctest --test-dir Log/build -C Release
#   cmake -S Log -B Log/build -DBUILD_LOG_TESTS=OFF    # 只编库、不编测试
#   cmake -S Log -B Log/build -DBUILD_LOG_EXAMPLE=OFF  # 不编示例
# ============================================================================

include(${CMAKE_CURRENT_LIST_DIR}/../Config.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/../CMake/StandaloneInit.cmake)

project(SanYiLog
    VERSION 1.0.0
    DESCRIPTION "SanYi CAD Log Library"
    LANGUAGES CXX
)

# MSVC 的 /W4 等选项必须在 project() 之后才能正确判定编译器
sanyi_setup_compiler_options()

# 示例目标仅存在于独立构建
option(BUILD_LOG_EXAMPLE "Build Log usage example" ON)

# --- 测试 -------------------------------------------------------------------
sanyi_only_own_tests(BUILD_LOG_TESTS)
if(BUILD_LOG_TESTS)
    sanyi_find_gtest()
endif()

# 内层：创建 Log DLL、挂载测试（BUILD_LOG_TESTS 时）、输出状态
add_subdirectory(Log)

# --- 示例（独立构建专属） ---------------------------------------------------
if(BUILD_LOG_EXAMPLE)
    add_subdirectory(Example)
endif()

# --- 独立构建摘要 -----------------------------------------------------------
message(STATUS "")
message(STATUS "========================================")
message(STATUS "  [Standalone: ${PROJECT_NAME}]")
message(STATUS "========================================")
message(STATUS "  Version:       ${PROJECT_VERSION}")
message(STATUS "  Build Tests:   ${BUILD_LOG_TESTS}")
message(STATUS "  Build Example: ${BUILD_LOG_EXAMPLE}")
message(STATUS "  Output:        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
message(STATUS "========================================")
message(STATUS "")
