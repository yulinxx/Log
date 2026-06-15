cmake_minimum_required(VERSION 3.20)

set(LIB_NAME "Log")

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

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" PREFIX "Header Files" FILES ${LOG_HEADERS})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" PREFIX "Source Files" FILES ${LOG_SOURCES})

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

message(STATUS "[${LIB_NAME}]")
message(STATUS "  Library: ${LIB_NAME}")
message(STATUS "  Version: 1.0.0")
message(STATUS "  Platform: ${CMAKE_SYSTEM_NAME}(${CMAKE_SYSTEM_PROCESSOR})")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "")