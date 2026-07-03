cmake_minimum_required(VERSION 3.20)

include(../Config.txt)
include(${CMAKE_CURRENT_LIST_DIR}/../CMake/StandaloneInit.cmake)

project(SanYiLog
    VERSION 1.0.0
    DESCRIPTION "SanYi CAD Log Library"
    LANGUAGES CXX
)

option(BUILD_LOG_TESTS "Build Log module tests" ON)

add_subdirectory(Log)

if(BUILD_LOG_TESTS AND GTest_FOUND)
    add_subdirectory(Test)
endif()

message(STATUS "[Standalone: ${PROJECT_NAME}]")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Tests: ${BUILD_LOG_TESTS}")
message(STATUS "----------------------------------------")
