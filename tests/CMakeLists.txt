# dwarfsql/tests/CMakeLists.txt - Internal tests
# These tests are run when building dwarfsql standalone

include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

# Copy test sources from monorepo tests location
# This allows the same tests to work both in monorepo and standalone builds
set(TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/dwarfsql/agent_settings_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/dwarfsql/dwarfsql_commands_test.cpp
)

# Check if monorepo tests exist, otherwise use local placeholder
foreach(src ${TEST_SOURCES})
    if(NOT EXISTS ${src})
        message(STATUS "Test source not found: ${src}")
        set(TEST_SOURCES "")
        break()
    endif()
endforeach()

if(TEST_SOURCES)
    add_executable(dwarfsql_internal_tests ${TEST_SOURCES})

    target_include_directories(dwarfsql_internal_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/common
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/include
    )

    target_link_libraries(dwarfsql_internal_tests PRIVATE
        GTest::gtest
        GTest::gtest_main
        xsql::xsql
    )

    if(DWARFSQL_WITH_AI_AGENT)
        target_link_libraries(dwarfsql_internal_tests PRIVATE libagents)
        target_compile_definitions(dwarfsql_internal_tests PRIVATE DWARFSQL_HAS_AI_AGENT)
    endif()

    include(GoogleTest)
    gtest_discover_tests(dwarfsql_internal_tests)
else()
    message(STATUS "dwarfsql: No test sources found, skipping internal tests")
endif()
