cmake_minimum_required(VERSION 4.3)

include(../Config.txt)

project(SanYiLog
    VERSION 1.0.0
    DESCRIPTION "SanYi CAD Log Library"
    LANGUAGES CXX
)

include(${CMAKE_CURRENT_LIST_DIR}/../CMake/StandaloneInit.cmake)

sanyi_find_gtest()

option(BUILD_LOG_TESTS "Build Log module tests" ON)
option(BUILD_LOG_EXAMPLE "Build Log usage example" ON)

add_subdirectory(Log)

if(BUILD_LOG_TESTS AND GTest_FOUND)
    add_subdirectory(Test)
endif()

if(BUILD_LOG_EXAMPLE)
    add_subdirectory(Example)
endif()

message(STATUS "[Standalone: ${PROJECT_NAME}]")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Tests: ${BUILD_LOG_TESTS}")
message(STATUS "----------------------------------------")
