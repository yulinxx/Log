cmake_minimum_required(VERSION 3.20)

# 查找 spdlog 库
find_package(spdlog CONFIG REQUIRED)

# 自动收集 Log 目录及子目录下的所有源文件和头文件
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

# 创建动态库
add_library(Log SHARED ${LOG_SOURCES} ${LOG_HEADERS})

# 定义导出宏
target_compile_definitions(Log PRIVATE LOG_EXPORTS)

# 设置包含目录
target_include_directories(Log
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/Include"
)

# 链接 spdlog
target_link_libraries(Log
    PRIVATE
        spdlog::spdlog
)

# 设置 C++ 标准
target_compile_features(Log PUBLIC cxx_std_17)

# MSVC 特定配置
if(MSVC)
    target_compile_options(Log PRIVATE /utf-8)
endif()

# 设置项目文件夹
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/Include" PREFIX "Header Files" FILES ${LOG_HEADERS})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/Src" PREFIX "Source Files" FILES ${LOG_SOURCES})
