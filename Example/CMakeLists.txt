add_executable(AprLogExample
    main.cpp
)

target_compile_features(AprLogExample PRIVATE cxx_std_17)

target_link_libraries(AprLogExample PRIVATE
    Log
)

if(MSVC)
    target_compile_options(AprLogExample PRIVATE /utf-8)
endif()

set_target_properties(AprLogExample PROPERTIES FOLDER "Examples")

message(STATUS "  Example: AprLogExample")
