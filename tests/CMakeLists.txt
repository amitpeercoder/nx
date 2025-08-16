# Test configuration
find_package(GTest REQUIRED)
find_package(benchmark REQUIRED)

# Enable all test types
enable_testing()

# Test libraries (shared code for all tests)
add_library(test_common STATIC
    common/test_helpers.cpp
    common/temp_directory.cpp
)

target_include_directories(test_common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/common
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_link_libraries(test_common PUBLIC
    GTest::gtest
    GTest::gtest_main
)

# Core library for tests
file(GLOB_RECURSE CLI_COMMAND_SOURCES "../src/cli/commands/*.cpp")
file(GLOB_RECURSE IMPORT_EXPORT_SOURCES "../src/import_export/*.cpp")
add_library(nx_core STATIC
    ../src/common.cpp
    ../src/core/note_id.cpp
    ../src/core/metadata.cpp
    ../src/core/note.cpp
    ../src/util/time.cpp
    ../src/util/xdg.cpp
    ../src/util/filesystem.cpp
    ../src/util/safe_process.cpp
    ../src/util/error_handler.cpp
    ../src/util/error_logger.cpp
    ../src/util/http_client.cpp
    ../src/util/security.cpp
    ../src/store/filesystem_store.cpp
    ../src/store/attachment_store.cpp
    ../src/store/filesystem_attachment_store.cpp
    ../src/store/notebook_manager.cpp
    ../src/config/config.cpp
    ../src/index/index.cpp
    ../src/index/sqlite_index.cpp
    ../src/index/query_parser.cpp
    ../src/index/ripgrep_index.cpp
    ../src/template/template_manager.cpp
    ../src/tui/tui_app.cpp
    ../src/tui/editor_security.cpp
    ../src/tui/editor_buffer.cpp
    ../src/tui/unicode_handler.cpp
    ../src/tui/editor_commands.cpp
    ../src/tui/enhanced_cursor.cpp
    ../src/tui/editor_search.cpp
    ../src/tui/editor_dialogs.cpp
    ../src/tui/viewport_manager.cpp
    ../src/tui/editor_preferences.cpp
    ../src/tui/editor_autocomplete.cpp
    ../src/tui/markdown_highlighter.cpp
    ../src/tui/markdown_shortcuts.cpp
    ../src/sync/git_sync.cpp
    ../src/crypto/age_crypto.cpp
    ../src/cli/application.cpp
    ../src/cli/command_error_handler.cpp
    ${CLI_COMMAND_SOURCES}
    ${IMPORT_EXPORT_SOURCES}
)

target_include_directories(nx_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${ICU_INCLUDE_DIRS}
)

target_link_libraries(nx_core PUBLIC
    CLI11::CLI11
    tomlplusplus::tomlplusplus
    yaml-cpp::yaml-cpp
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    ${SQLITE3_LIBRARIES}
    CURL::libcurl
    ${FTXUI_LIBRARIES}
    ${ICU_LIBRARIES}
)

# Platform-specific libraries
if(APPLE)
    target_link_libraries(nx_core PUBLIC "-framework Security")
endif()

# Link git2 with library paths
if(LIBGIT2_FOUND)
    target_link_libraries(nx_core PUBLIC ${LIBGIT2_LIBRARIES})
    target_link_directories(nx_core PUBLIC ${LIBGIT2_LIBRARY_DIRS})
else()
    target_link_libraries(nx_core PUBLIC ${GIT2_LIBRARIES})
endif()

# Unit tests
file(GLOB_RECURSE UNIT_TEST_SOURCES "unit/*.cpp")
if(UNIT_TEST_SOURCES)
    add_executable(unit_tests ${UNIT_TEST_SOURCES})
    target_link_libraries(unit_tests PRIVATE
        nx_core
        test_common
        GTest::gtest
        GTest::gtest_main
    )
    
    # Add individual test discovery
    include(GoogleTest)
    gtest_discover_tests(unit_tests
        PROPERTIES LABELS "unit"
    )
endif()

# Integration tests
file(GLOB_RECURSE INTEGRATION_TEST_SOURCES "integration/*.cpp")
if(INTEGRATION_TEST_SOURCES)
    add_executable(integration_tests ${INTEGRATION_TEST_SOURCES})
    target_link_libraries(integration_tests PRIVATE
        nx_core
        test_common
        GTest::gtest
        GTest::gtest_main
    )
    
    gtest_discover_tests(integration_tests
        PROPERTIES LABELS "integration"
    )
endif()

# Benchmark tests
file(GLOB_RECURSE BENCHMARK_SOURCES "benchmark/*.cpp")
if(BENCHMARK_SOURCES)
    add_executable(nx_benchmark ${BENCHMARK_SOURCES})
    target_link_libraries(nx_benchmark PRIVATE
        nx_core
        test_common
        benchmark::benchmark
        benchmark::benchmark_main
    )
    
    # Register benchmark as test (but don't run in normal test suite)
    add_test(NAME benchmark_tests
        COMMAND nx_benchmark --benchmark_min_time=0.1
    )
    set_tests_properties(benchmark_tests PROPERTIES LABELS "benchmark")
endif()

# Fuzz tests (if compiler supports it)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    file(GLOB_RECURSE FUZZ_TEST_SOURCES "fuzz/*.cpp")
    if(FUZZ_TEST_SOURCES)
        foreach(fuzz_source ${FUZZ_TEST_SOURCES})
            get_filename_component(fuzz_name ${fuzz_source} NAME_WE)
            add_executable(${fuzz_name} ${fuzz_source})
            target_compile_options(${fuzz_name} PRIVATE
                -fsanitize=fuzzer,address
                -g -O1
            )
            target_link_options(${fuzz_name} PRIVATE
                -fsanitize=fuzzer,address
            )
            target_link_libraries(${fuzz_name} PRIVATE test_common)
        endforeach()
    endif()
endif()

# Test data and corpus
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/)

# Custom test targets
add_custom_target(test_unit
    COMMAND ${CMAKE_CTEST_COMMAND} -L unit --output-on-failure
    DEPENDS unit_tests
    COMMENT "Running unit tests"
)

add_custom_target(test_integration
    COMMAND ${CMAKE_CTEST_COMMAND} -L integration --output-on-failure
    DEPENDS integration_tests
    COMMENT "Running integration tests"
)

add_custom_target(test_all
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS unit_tests integration_tests
    COMMENT "Running all tests"
)