add_executable(SyLogExample
    main.cpp
)

target_compile_features(SyLogExample PRIVATE cxx_std_17)

target_link_libraries(SyLogExample PRIVATE
    Log
)

if(MSVC)
    target_compile_options(SyLogExample PRIVATE /utf-8)
endif()

set_target_properties(SyLogExample PROPERTIES FOLDER "Examples")

message(STATUS "  Example: SyLogExample")
