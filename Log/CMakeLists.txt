cmake_minimum_required(VERSION 4.3)

# --- 模块元信息 ---
set(LIB_NAME "Log")
set(LIB_FOLDER "Core")
set(LIB_DESCRIPTION "Logging Library")
set(LIB_EXPORT_MACRO "LOG_EXPORTS")

# --- 版本（按模块独立）---
set(VERSION_MAJOR 1)
set(VERSION_MINOR 0)
set(VERSION_PATCH 0)
set(PROJECT_VERSION "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")

# --- 路径 ---
if(NOT SANYI_ROOT)
    get_filename_component(SANYI_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
endif()
include("${SANYI_ROOT}/CMake/SanYiPaths.cmake")
include("${SANYI_ROOT}/CMake/Utils.cmake")

# --- 源文件 ---
file(GLOB_RECURSE LIB_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Src/*.cxx"
    "${CMAKE_CURRENT_SOURCE_DIR}/Src/*.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/Src/*.c"
)
file(GLOB_RECURSE LIB_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Include/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/Include/*.hpp"
)
list(FILTER LIB_SOURCES EXCLUDE REGEX "${SANYI_SOURCE_EXCLUDE_REGEX}")
list(FILTER LIB_HEADERS EXCLUDE REGEX "${SANYI_HEADER_EXCLUDE_REGEX}")

find_package(spdlog CONFIG REQUIRED)

# --- 统一入口创建 DLL ---
sanyi_add_shared_library(${LIB_NAME}
    FOLDER "${LIB_FOLDER}"
    DESCRIPTION "${LIB_DESCRIPTION}"
    EXPORT_MACRO "${LIB_EXPORT_MACRO}"
    VERSION_MAJOR ${VERSION_MAJOR}
    VERSION_MINOR ${VERSION_MINOR}
    VERSION_PATCH ${VERSION_PATCH}
    SOURCES ${LIB_SOURCES}
    HEADERS ${LIB_HEADERS}
    PUBLIC_INCLUDE_DIRS
        "${CMAKE_CURRENT_SOURCE_DIR}/Include"
    LINK_LIBRARIES
        PRIVATE spdlog::spdlog
)

# --- 单元测试 ---
# 注意：根 CMakeLists 直接 add_subdirectory(Log/Log)，不会经过父级 Log/CMakeLists.txt，
# 因此测试必须挂在这里，否则 BUILD_LOG_TESTS=ON 也永远不生成 LogTests 目标。
if(BUILD_LOG_TESTS)
    enable_testing()
    find_package(GTest CONFIG REQUIRED)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../Test" "${CMAKE_CURRENT_BINARY_DIR}/Test")
endif()

message(STATUS "[${LIB_NAME}]")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Platform: ${CMAKE_SYSTEM_NAME}(${CMAKE_SYSTEM_PROCESSOR})")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "----------------------------------------")

sanyi_export_imported_target(${LIB_NAME}
    LIB_NAME "${LIB_NAME}"
    PUBLIC_INCLUDE_DIRS
        "${CMAKE_CURRENT_SOURCE_DIR}/Include"
)