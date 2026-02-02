# Log模块单元测试配置

# 查找GTest包
find_package(GTest REQUIRED)

# 包含GoogleTest模块以使用gtest_discover_tests
include(GoogleTest)

# 收集测试源文件
file(GLOB_RECURSE LOG_TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cxx"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
)

# 创建测试可执行文件
add_executable(LogTests ${LOG_TEST_SOURCES})

# 设置编译特性
target_compile_features(LogTests PRIVATE cxx_std_17)

# 添加包含目录
target_include_directories(LogTests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/.."
    "${CMAKE_CURRENT_SOURCE_DIR}/../Include"
)

# 链接依赖库
target_link_libraries(LogTests PRIVATE
    Log
    GTest::gtest
    GTest::gtest_main
    GTest::gmock
    GTest::gmock_main
)

# 设置编译定义
target_compile_definitions(LogTests PRIVATE LOG_EXTERN_TEMPLATES)

# 将测试添加到CTest
gtest_discover_tests(LogTests)

# 启用测试报告（可选）
set_property(TARGET LogTests PROPERTY FOLDER "Tests")

# MSVC特定配置
if(MSVC)
    target_compile_options(LogTests PRIVATE /utf-8)
endif()
