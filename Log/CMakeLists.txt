cmake_minimum_required(VERSION 3.20)

set(LIB_NAME "Log")
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../CMake/Utils.cmake")
    include("${CMAKE_CURRENT_SOURCE_DIR}/../../CMake/Utils.cmake")
endif()

# 版本号定义
set(VERSION_MAJOR 1)
set(VERSION_MINOR 0)
set(VERSION_PATCH 0)
set(PROJECT_VERSION_MAJOR ${VERSION_MAJOR})
set(PROJECT_VERSION_MINOR ${VERSION_MINOR})
set(PROJECT_VERSION_PATCH ${VERSION_PATCH})
set(PROJECT_VERSION "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")

find_package(spdlog CONFIG REQUIRED)

file(GLOB_RECURSE LOG_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Src/*.cxx"
    "${CMAKE_CURRENT_SOURCE_DIR}/Src/*.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/Src/*.c"
)

file(GLOB_RECURSE LOG_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Include/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/Include/*.hpp"
)

add_library(${LIB_NAME} SHARED
    ${LOG_SOURCES}
    ${LOG_HEADERS}
)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${LOG_SOURCES} ${LOG_HEADERS})

target_include_directories(${LIB_NAME}
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/Include"
)

target_compile_features(${LIB_NAME} PUBLIC cxx_std_17)

target_link_libraries(${LIB_NAME}
    PRIVATE
        spdlog::spdlog
)

target_compile_definitions(${LIB_NAME} PRIVATE LOG_EXPORTS)

if(MSVC)
    target_compile_options(${LIB_NAME} PRIVATE /utf-8 /FS)
endif()

sanyi_add_version_info(${LIB_NAME} "${LIB_NAME}" "Logging Library")

message(STATUS "[${LIB_NAME}]")
message(STATUS "  Library: ${LIB_NAME}")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Platform: ${CMAKE_SYSTEM_NAME}(${CMAKE_SYSTEM_PROCESSOR})")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "")
message(STATUS "----------------------------------------")