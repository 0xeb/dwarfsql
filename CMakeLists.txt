# dwarfsql - SQL interface to DWARF debug information
#
# Build: cmake -B build -DDWARFSQL_WITH_AI_AGENT=ON && cmake --build build
# Test:  ctest --test-dir build --output-on-failure

cmake_minimum_required(VERSION 3.20)
project(dwarfsql VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Options
option(DWARFSQL_WITH_AI_AGENT "Enable AI agent support" ON)
option(DWARFSQL_BUILD_TESTS "Build tests" ON)

# Output directories (only if standalone build)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
endif()

# ============================================================================
# xsql dependency
# ============================================================================

if(TARGET xsql::xsql)
    message(STATUS "dwarfsql: Using xsql::xsql from parent project")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../libxsql/CMakeLists.txt")
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../libxsql ${CMAKE_CURRENT_BINARY_DIR}/libxsql)
    message(STATUS "dwarfsql: Added libxsql from monorepo")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/libxsql/CMakeLists.txt")
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/external/libxsql)
    message(STATUS "dwarfsql: Added libxsql from external submodule")
else()
    find_package(xsql QUIET)
    if(NOT xsql_FOUND)
        message(FATAL_ERROR "libxsql not found. Options:\n"
            "  1. Build as part of xsql monorepo\n"
            "  2. Add as submodule: git submodule add ../libxsql external/libxsql\n"
            "  3. Install libxsql and use find_package(xsql)")
    endif()
endif()

# ============================================================================
# libdwarf dependency
# ============================================================================

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(LIBDWARF QUIET libdwarf)
endif()

if(NOT LIBDWARF_FOUND)
    # Try to find libdwarf manually
    find_path(LIBDWARF_INCLUDE_DIR
        NAMES libdwarf/libdwarf.h dwarf.h libdwarf.h
        PATHS
            /usr/include
            /usr/local/include
            /opt/homebrew/include
            ${CMAKE_CURRENT_SOURCE_DIR}/external/libdwarf/include
    )

    find_library(LIBDWARF_LIBRARY
        NAMES dwarf libdwarf
        PATHS
            /usr/lib
            /usr/lib64
            /usr/local/lib
            /opt/homebrew/lib
            ${CMAKE_CURRENT_SOURCE_DIR}/external/libdwarf/lib
    )

    if(LIBDWARF_INCLUDE_DIR AND LIBDWARF_LIBRARY)
        set(LIBDWARF_FOUND TRUE)
        set(LIBDWARF_INCLUDE_DIRS ${LIBDWARF_INCLUDE_DIR})
        set(LIBDWARF_LIBRARIES ${LIBDWARF_LIBRARY})
        message(STATUS "dwarfsql: Found libdwarf: ${LIBDWARF_LIBRARY}")
    else()
        message(WARNING "libdwarf not found. Install with:\n"
            "  Ubuntu/Debian: apt install libdwarf-dev\n"
            "  macOS: brew install libdwarf\n"
            "  Fedora: dnf install libdwarf-devel\n"
            "Building stub library for now...")
        set(DWARFSQL_STUB_MODE ON)
    endif()
endif()

# Also need libelf on most systems
find_library(LIBELF_LIBRARY
    NAMES elf libelf
    PATHS /usr/lib /usr/lib64 /usr/local/lib /opt/homebrew/lib
)

# ============================================================================
# libagents (optional AI agent support)
# ============================================================================

if(DWARFSQL_WITH_AI_AGENT)
    if(TARGET libagents)
        message(STATUS "dwarfsql: Using libagents from parent project")
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../idasql/external/libagents/CMakeLists.txt")
        set(LIBAGENTS_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(LIBAGENTS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(LIBAGENTS_BUILD_CLI OFF CACHE BOOL "" FORCE)
        set(LIBAGENTS_BUILD_COPILOT ON CACHE BOOL "" FORCE)
        add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../idasql/external/libagents
                         ${CMAKE_CURRENT_BINARY_DIR}/libagents)
        message(STATUS "dwarfsql: Added libagents from idasql/external")
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/libagents/CMakeLists.txt")
        set(LIBAGENTS_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(LIBAGENTS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(LIBAGENTS_BUILD_CLI OFF CACHE BOOL "" FORCE)
        set(LIBAGENTS_BUILD_COPILOT ON CACHE BOOL "" FORCE)
        add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/external/libagents)
        message(STATUS "dwarfsql: Added libagents from external submodule")
    else()
        message(WARNING "libagents not found, disabling AI agent support")
        set(DWARFSQL_WITH_AI_AGENT OFF)
    endif()
endif()

# ============================================================================
# Embed system prompt (if AI agent enabled)
# ============================================================================

if(DWARFSQL_WITH_AI_AGENT)
    set(PROMPT_MD "${CMAKE_CURRENT_SOURCE_DIR}/prompts/dwarfsql_agent.md")
    set(PROMPT_HPP "${CMAKE_CURRENT_SOURCE_DIR}/src/common/dwarfsql_agent_prompt.hpp")
    add_custom_command(
        OUTPUT ${PROMPT_HPP}
        COMMAND ${CMAKE_COMMAND} -E env python3 "${CMAKE_CURRENT_SOURCE_DIR}/scripts/embed_prompt.py"
                "${PROMPT_MD}" "${PROMPT_HPP}"
        DEPENDS ${PROMPT_MD}
        COMMENT "Embedding dwarfsql_agent.md into dwarfsql_agent_prompt.hpp"
        VERBATIM
    )
    add_custom_target(dwarfsql_embed_prompt DEPENDS ${PROMPT_HPP})
endif()

# ============================================================================
# dwarfsql library
# ============================================================================

add_library(dwarfsql_lib STATIC
    src/dwarf_session.cpp
    src/dwarf_tables.cpp
)
add_library(dwarfsql::dwarfsql ALIAS dwarfsql_lib)

target_include_directories(dwarfsql_lib PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(dwarfsql_lib PUBLIC xsql::xsql)

if(LIBDWARF_FOUND AND NOT DWARFSQL_STUB_MODE)
    target_include_directories(dwarfsql_lib PUBLIC ${LIBDWARF_INCLUDE_DIRS})
    target_link_libraries(dwarfsql_lib PUBLIC ${LIBDWARF_LIBRARIES})
    if(LIBELF_LIBRARY)
        target_link_libraries(dwarfsql_lib PUBLIC ${LIBELF_LIBRARY})
    endif()
    target_compile_definitions(dwarfsql_lib PUBLIC DWARFSQL_HAS_LIBDWARF)
endif()

target_compile_features(dwarfsql_lib PUBLIC cxx_std_17)

# ============================================================================
# dwarfsql CLI
# ============================================================================

set(DWARFSQL_CLI_SOURCES src/cli/main.cpp)

if(DWARFSQL_WITH_AI_AGENT)
    list(APPEND DWARFSQL_CLI_SOURCES src/common/ai_agent.cpp)
endif()

add_executable(dwarfsql ${DWARFSQL_CLI_SOURCES})

target_include_directories(dwarfsql PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src/common
)

target_link_libraries(dwarfsql PRIVATE dwarfsql_lib)

if(DWARFSQL_WITH_AI_AGENT)
    add_dependencies(dwarfsql dwarfsql_embed_prompt)
    target_link_libraries(dwarfsql PRIVATE libagents)
    target_compile_definitions(dwarfsql PRIVATE DWARFSQL_HAS_AI_AGENT)
endif()

if(WIN32)
    target_link_libraries(dwarfsql PRIVATE ws2_32)
endif()

# ============================================================================
# Tests
# ============================================================================

if(DWARFSQL_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ============================================================================
# Install
# ============================================================================

include(GNUInstallDirs)

install(TARGETS dwarfsql dwarfsql_lib
    EXPORT dwarfsqlTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(DIRECTORY src/include/dwarfsql
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
