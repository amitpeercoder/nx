cmake_minimum_required(VERSION 3.25)

# Project definition
project(nx
    VERSION 0.1.0
    DESCRIPTION "High-performance CLI notes application"
    LANGUAGES CXX
)

# C++ standard requirements
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Require C++23
set(CMAKE_CXX_STANDARD 23)

message(STATUS "Using C++${CMAKE_CXX_STANDARD}")

# Build type defaults
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

# Use Homebrew paths on macOS
if(APPLE)
    set(CMAKE_PREFIX_PATH "/opt/homebrew" ${CMAKE_PREFIX_PATH})
endif()

# vcpkg integration (optional)
if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "")
endif()

# Compiler flags
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    # Warning flags
    set(WARNING_FLAGS
        -Wall -Wextra -Wpedantic
        -Wconversion -Wsign-conversion
        -Wcast-align -Wcast-qual
        -Wdouble-promotion
        -Wformat=2
        -Wnull-dereference
        -Wold-style-cast
        -Woverloaded-virtual
        -Wshadow
        -Wunused
        -Wno-unused-parameter
    )
    
    # Additional GCC warnings
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        list(APPEND WARNING_FLAGS
            -Wduplicated-cond
            -Wduplicated-branches
            -Wlogical-op
            -Wrestrict
        )
    endif()
    
    # Additional Clang warnings
    if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        list(APPEND WARNING_FLAGS
            -Wloop-analysis
            -Wmove
        )
    endif()
endif()

# Sanitizer options
option(SANITIZE_ADDRESS "Enable AddressSanitizer" OFF)
option(SANITIZE_UNDEFINED "Enable UndefinedBehaviorSanitizer" OFF)
option(SANITIZE_THREAD "Enable ThreadSanitizer" OFF)

if(SANITIZE_ADDRESS)
    set(SANITIZER_FLAGS "-fsanitize=address -fno-omit-frame-pointer")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${SANITIZER_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${SANITIZER_FLAGS}")
endif()

if(SANITIZE_UNDEFINED)
    set(SANITIZER_FLAGS "-fsanitize=undefined")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${SANITIZER_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${SANITIZER_FLAGS}")
endif()

if(SANITIZE_THREAD)
    set(SANITIZER_FLAGS "-fsanitize=thread")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${SANITIZER_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${SANITIZER_FLAGS}")
endif()

# Find dependencies
find_package(PkgConfig REQUIRED)
find_package(CLI11 CONFIG REQUIRED)
find_package(tomlplusplus CONFIG REQUIRED)
find_package(yaml-cpp CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

# Try to find FTXUI via CONFIG first, then pkg-config
find_package(ftxui CONFIG QUIET)
if(ftxui_FOUND)
    set(FTXUI_LIBRARIES ftxui::screen ftxui::dom ftxui::component)
else()
    pkg_check_modules(FTXUI REQUIRED ftxui)
    # Link to full library paths
    set(FTXUI_LIBRARIES 
        ${CMAKE_PREFIX_PATH}/lib/libftxui-screen.dylib
        ${CMAKE_PREFIX_PATH}/lib/libftxui-dom.dylib
        ${CMAKE_PREFIX_PATH}/lib/libftxui-component.dylib
    )
endif()

# Try to find libgit2 via pkg-config first, then CONFIG
pkg_check_modules(LIBGIT2 libgit2)
if(LIBGIT2_FOUND)
    set(GIT2_LIBRARIES ${LIBGIT2_LIBRARIES})
    set(GIT2_INCLUDE_DIRS ${LIBGIT2_INCLUDE_DIRS})
else()
    find_package(git2 CONFIG QUIET)
    if(git2_FOUND)
        set(GIT2_LIBRARIES git2::git2)
    else()
        # Fallback: assume system install
        find_library(GIT2_LIBRARIES NAMES git2 libgit2 REQUIRED)
        find_path(GIT2_INCLUDE_DIRS git2.h REQUIRED)
    endif()
endif()

# SQLite with FTS5
pkg_check_modules(SQLITE3 REQUIRED sqlite3>=3.38)

# HTTP client for AI API calls
find_package(CURL REQUIRED)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Source files
file(GLOB_RECURSE SOURCES
    "src/*.cpp"
    "src/app/*.cpp"
    "src/cli/*.cpp"
    "src/tui/*.cpp"
    "src/core/*.cpp"
    "src/store/*.cpp"
    "src/index/*.cpp"
    "src/sync/*.cpp"
    "src/crypto/*.cpp"
    "src/import_export/*.cpp"
    "src/util/*.cpp"
    "src/config/*.cpp"
)

# Main executable
add_executable(nx ${SOURCES})

# Link libraries
target_link_libraries(nx PRIVATE
    CLI11::CLI11
    tomlplusplus::tomlplusplus
    yaml-cpp::yaml-cpp
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    ${SQLITE3_LIBRARIES}
    CURL::libcurl
    ${FTXUI_LIBRARIES}
)

# Platform-specific libraries
if(APPLE)
    find_library(SECURITY_FRAMEWORK Security REQUIRED)
    target_link_libraries(nx PRIVATE ${SECURITY_FRAMEWORK})
endif()

# Link git2 with library paths
if(LIBGIT2_FOUND)
    target_link_libraries(nx PRIVATE ${LIBGIT2_LIBRARIES})
    target_link_directories(nx PRIVATE ${LIBGIT2_LIBRARY_DIRS})
else()
    target_link_libraries(nx PRIVATE ${GIT2_LIBRARIES})
endif()

# Include directories for dependencies
target_include_directories(nx PRIVATE 
    ${SQLITE3_INCLUDE_DIRS}
    ${GIT2_INCLUDE_DIRS}
    ${FTXUI_INCLUDE_DIRS}
)

# Compiler definitions
target_compile_definitions(nx PRIVATE ${SQLITE3_CFLAGS_OTHER})

# Apply warning flags
target_compile_options(nx PRIVATE ${WARNING_FLAGS})

# Enable testing
enable_testing()

# Add subdirectories
add_subdirectory(tests)

# Install targets
install(TARGETS nx
    RUNTIME DESTINATION bin
)

# Install man page (when created)
# install(FILES docs/nx.1 DESTINATION share/man/man1)

# CPack configuration
include(InstallRequiredSystemLibraries)
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_VERSION_MAJOR "${PROJECT_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${PROJECT_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${PROJECT_VERSION_PATCH}")
set(CPACK_PACKAGE_CONTACT "nx-dev@example.com")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")

# Debian package
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libsqlite3-0 (>= 3.38)")
set(CPACK_DEBIAN_PACKAGE_SECTION "utils")

# RPM package
set(CPACK_RPM_PACKAGE_REQUIRES "sqlite >= 3.38")
set(CPACK_RPM_PACKAGE_GROUP "Applications/Productivity")

# Package generators
set(CPACK_GENERATOR "DEB;RPM;TGZ")

include(CPack)