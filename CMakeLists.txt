#
# Copyright (c) 2024-2026 Elias Bachaalany
# SPDX-License-Identifier: LicenseRef-Human-Origin-Source-1.0
#
# This file is licensed under the Human-Origin Source License v1.0.
# See LICENSE.
#

# dwarfsql - SQL interface to DWARF debug information
#
# Build: cmake -B build && cmake --build build

cmake_minimum_required(VERSION 3.20)
project(dwarfsql VERSION 0.0.3 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Windows VERSIONINFO wiring (version is from project() above; no-op on non-Windows)
set(TOOL_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(TOOL_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(TOOL_VERSION_PATCH ${PROJECT_VERSION_PATCH})
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/versioninfo.cmake)

# Options
option(DWARFSQL_WITH_HTTP "Build with HTTP REST server support" ON)
option(DWARFSQL_WITH_MCP "Build with MCP server support (fastmcpp)" ON)

# HTTP support requires thinclient from libxsql
if(DWARFSQL_WITH_HTTP)
    message(STATUS "dwarfsql: Building with HTTP server support")
    set(XSQL_WITH_THINCLIENT ON CACHE BOOL "" FORCE)
endif()

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
    message(STATUS "dwarfsql: Added libxsql from sibling directory")
else()
    message(STATUS "dwarfsql: Fetching libxsql from GitHub...")
    include(FetchContent)
    FetchContent_Declare(libxsql
        GIT_REPOSITORY https://github.com/0xeb/libxsql.git
        GIT_TAG        main
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(libxsql)
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
# MCP support (optional, uses fastmcpp)
# ============================================================================

if(DWARFSQL_WITH_MCP)
    message(STATUS "dwarfsql: Building with MCP support via fastmcpp")
    if(NOT TARGET fastmcpp_core)
        set(FASTMCPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(FASTMCPP_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(FASTMCPP_ENABLE_POST_STREAMING OFF CACHE BOOL "" FORCE)
        set(FASTMCPP_ENABLE_SAMPLING_HTTP_HANDLERS OFF CACHE BOOL "" FORCE)
        set(FASTMCPP_FETCH_CURL OFF CACHE BOOL "" FORCE)
        message(STATUS "dwarfsql: Fetching fastmcpp from GitHub...")
        include(FetchContent)
        FetchContent_Declare(fastmcpp
            GIT_REPOSITORY https://github.com/0xeb/fastmcpp.git
            GIT_TAG        main
            GIT_SHALLOW    TRUE
        )
        FetchContent_MakeAvailable(fastmcpp)
    endif()
    if(MSVC AND TARGET fastmcpp_core)
        target_compile_options(fastmcpp_core PRIVATE /MP)
    endif()
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

if(DWARFSQL_WITH_MCP)
    list(APPEND DWARFSQL_CLI_SOURCES
        src/common/mcp_server.cpp
    )
endif()

if(DWARFSQL_WITH_HTTP)
    list(APPEND DWARFSQL_CLI_SOURCES src/common/http_server.cpp)
endif()

add_executable(dwarfsql ${DWARFSQL_CLI_SOURCES})

# Windows VS_VERSION_INFO resource (no-op on non-Windows)
xsql_add_versioninfo(dwarfsql
    FILETYPE          VFT_APP
    DESCRIPTION       "dwarfsql command-line interface for DWARF debug info"
    ORIGINAL_FILENAME "dwarfsql.exe")

target_include_directories(dwarfsql PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src/common
)

target_link_libraries(dwarfsql PRIVATE dwarfsql_lib)

if(DWARFSQL_WITH_MCP)
    target_link_libraries(dwarfsql PRIVATE fastmcpp_core)
    target_compile_definitions(dwarfsql PRIVATE DWARFSQL_HAS_MCP)
endif()

if(DWARFSQL_WITH_HTTP)
    target_compile_definitions(dwarfsql PRIVATE DWARFSQL_HAS_HTTP)
endif()

if(WIN32)
    target_link_libraries(dwarfsql PRIVATE ws2_32)
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
