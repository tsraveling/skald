cmake_minimum_required(VERSION 3.5)

project(skald)

# Include PEGTL
add_subdirectory(deps/pegtl)

# Main library
add_library(skald STATIC
    src/skald.cpp
    include/skald.h
    # Add other source files as needed
)

target_include_directories(skald 
    PUBLIC 
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Link PEGTL (header-only, so just need include directories)
target_link_libraries(skald PUBLIC taocpp::pegtl)

# Optional: Executable for testing purposes only
option(SKALD_BUILD_TEST_EXECUTABLE "Build a test executable for skald" ON)

if(SKALD_BUILD_TEST_EXECUTABLE)
    add_executable(skald_test src/test_main.cpp)
    target_link_libraries(skald_test PRIVATE skald)
endif()
