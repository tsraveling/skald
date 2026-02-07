cmake_minimum_required(VERSION 3.24)

project(skald VERSION 0.3.2 LANGUAGES C CXX)

# Generate version header
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/include/skald_version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/include/skald_version.h
    @ONLY
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Options
option(SKALD_BUILD_TEST_EXECUTABLE "Build test executable" OFF)
option(SKALD_BUILD_SKALDER "Build skalder TUI tool" ON)
option(SKALD_BUILD_SHARED "Build shared library with C bindings" ON)

# Include PEGTL
add_subdirectory(deps/pegtl)

# Core sources (used by both static and shared builds)
set(SKALD_CORE_SOURCES
    src/debug.cpp
    src/skald.cpp
)

# =============================================================================
# Static library (your existing internal C++ library)
# =============================================================================
add_library(skald_static STATIC ${SKALD_CORE_SOURCES})
add_library(skald::static ALIAS skald_static)

target_include_directories(skald_static
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(skald_static PUBLIC taocpp::pegtl)

# Keep the old name for backwards compatibility
add_library(skald ALIAS skald_static)

# =============================================================================
# Shared library with C bindings (for FFI consumers: Go, Godot, etc.)
# =============================================================================
if(SKALD_BUILD_SHARED)
    add_library(skald_shared SHARED
        ${SKALD_CORE_SOURCES}
        bindings/c/skald_c.cpp
    )
    add_library(skald::shared ALIAS skald_shared)

    # Output name is just "skald" -> libskald.so / libskald.dylib / skald.dll
    set_target_properties(skald_shared PROPERTIES
        OUTPUT_NAME skald
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
        # Hide internal symbols, only export what's marked SKALD_API
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
    )

    target_include_directories(skald_shared
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/bindings/c>
            $<INSTALL_INTERFACE:include>
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    target_compile_definitions(skald_shared
        PRIVATE
            SKALD_BUILD      # We're building the library (for dllexport)
            SKALD_SHARED     # Enable visibility macros
    )

    target_link_libraries(skald_shared PRIVATE taocpp::pegtl)
endif()

# =============================================================================
# Test executable and tests 
# =============================================================================
if(SKALD_BUILD_TEST_EXECUTABLE)
    add_executable(skald_test src/test_main.cpp)
    target_link_libraries(skald_test PRIVATE skald_static)
endif()

if(SKALD_BUILD_SKALDER)
  include(FetchContent)

    FetchContent_Declare(ftxui
        GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
        GIT_TAG v6.1.9
    )
    FetchContent_MakeAvailable(ftxui)

    add_executable(skalder src/skalder.cpp)
    target_link_libraries(skalder PRIVATE skald_static ftxui::screen ftxui::dom ftxui::component)
endif()

option(SKALD_BUILD_C_TEST "Build C API test" OFF)

if(SKALD_BUILD_C_TEST AND SKALD_BUILD_SHARED)
    add_executable(test_c_api bindings/c/test_c_api.c)
    target_link_libraries(test_c_api PRIVATE skald_shared)
    target_include_directories(test_c_api PRIVATE bindings/c)
    # Force C++ linker since we're linking against a C++ library
    set_target_properties(test_c_api PROPERTIES LINKER_LANGUAGE CXX)
endif()

# =============================================================================
# Installation
# =============================================================================
include(GNUInstallDirs)

# Install the C header for FFI consumers
install(FILES bindings/c/skald_c.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Install shared library
if(SKALD_BUILD_SHARED)
    install(TARGETS skald_shared
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}  # For Windows DLLs
    )
endif()

# Optionally install the C++ header and static lib (for C++ consumers)
install(FILES include/skald.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/include/skald_version.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(TARGETS skald_static
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
