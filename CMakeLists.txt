cmake_minimum_required(VERSION 3.20)

include(../Config.txt)

# 设置VCPKG工具链
if(DEFINED VCPKG_DIR AND EXISTS "${VCPKG_DIR}")
    set(CMAKE_TOOLCHAIN_FILE "${VCPKG_DIR}/scripts/buildsystems/vcpkg.cmake")
    message(STATUS "Using VCPKG toolchain: ${CMAKE_TOOLCHAIN_FILE}")
else()
    message(WARNING "VCPKG directory error. Please set VCPKG_DIR in Config.txt")
endif()

# 项目基本信息
project(SanYiLog
    VERSION 1.0.0
    DESCRIPTION "SanYi CAD Log Library"
    LANGUAGES CXX
)

# 设置项目标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 设置输出目录
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# 编译器特定设置
if(MSVC)
    # MSVC编译器设置
    add_compile_options(/W4 /utf-8)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    # 禁用特定警告
    add_compile_options(/wd4251)  # 禁用'class needs to have dll-interface'警告
    add_compile_options(/wd4244)  # 禁用类型转换警告（double到int转换）
else()
    # GCC/Clang编译器设置
    add_compile_options(-Wall -Wextra -Wpedantic)
    # 禁用特定警告
    add_compile_options(-Wno-conversion)  # 禁用类型转换警告
    add_compile_options(-Wno-float-conversion)  # 禁用浮点转换警告
endif()

# 构建类型配置
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# 设置构建选项
option(BUILD_SHARED_LIBS "Build shared libraries" ON)
option(BUILD_LOG_TESTS "Build Log module tests" ON)

# 添加子目录
add_subdirectory(Log)

# 单元测试
if(BUILD_LOG_TESTS)
    enable_testing()
    find_package(GTest REQUIRED)
    add_subdirectory(Test)
endif()

message(STATUS "[${LIB_NAME}]")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Shared Libraries: ${BUILD_SHARED_LIBS}")
message(STATUS "  Tests: ${BUILD_LOG_TESTS}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "")
