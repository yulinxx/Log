# Log模块单元测试配置

# 查找GTest包
find_package(GTest REQUIRED)

# 查找spdlog包
find_package(spdlog CONFIG REQUIRED)

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
    spdlog::spdlog
)

# 设置编译定义
target_compile_definitions(LogTests PRIVATE LOG_EXTERN_TEMPLATES)

# MSVC特定配置
if(MSVC)
    target_compile_options(LogTests PRIVATE /utf-8)
endif()

# 将测试添加到CTest（使用add_test替代gtest_discover_tests以控制环境变量）
# 通过设置PATH环境变量确保测试发现和运行时能找到依赖DLL
if(WIN32)
    set(VCPKG_BIN_DIR "${VCPKG_DIR}/installed/x64-windows/bin")
    set(VCPKG_DEBUG_BIN_DIR "${VCPKG_DIR}/installed/x64-windows/debug/bin")
    set(LOG_TESTS_PATH "${VCPKG_DEBUG_BIN_DIR};${VCPKG_BIN_DIR};$ENV{PATH}")
    
    add_test(NAME LogTests
        COMMAND "${CMAKE_BINARY_DIR}/bin_Qt${QT_VERSION_MAJOR}/$<CONFIG>/LogTests.exe"
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin_Qt${QT_VERSION_MAJOR}/$<CONFIG>"
    )
    set_tests_properties(LogTests PROPERTIES
        ENVIRONMENT "PATH=${LOG_TESTS_PATH}"
    )
else()
    gtest_discover_tests(LogTests)
endif()

# 启用测试报告（可选）
set_property(TARGET LogTests PROPERTY FOLDER "Tests")
