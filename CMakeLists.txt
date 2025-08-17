cmake_minimum_required(VERSION 3.25)

# Include git versioning system
include(cmake/GitVersion.cmake)

# Get git version information
get_git_version()

# Project definition with git-based version
project(nx
    VERSION ${GIT_VERSION_MAJOR}.${GIT_VERSION_MINOR}.${GIT_VERSION_PATCH}
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

# Try to find FTXUI via CONFIG first, then pkg-config, then manual search
find_package(ftxui CONFIG QUIET)
if(ftxui_FOUND)
    set(FTXUI_LIBRARIES ftxui::screen ftxui::dom ftxui::component)
else()
    pkg_check_modules(FTXUI QUIET ftxui)
    if(FTXUI_FOUND)
        set(FTXUI_LIBRARIES ${FTXUI_LIBRARIES})
    else()
        # Manual search for FTXUI libraries (cross-platform)
        find_library(FTXUI_SCREEN_LIB 
            NAMES ftxui-screen libftxui-screen
            PATHS ${CMAKE_PREFIX_PATH}/lib /usr/local/lib /opt/homebrew/lib
        )
        find_library(FTXUI_DOM_LIB 
            NAMES ftxui-dom libftxui-dom  
            PATHS ${CMAKE_PREFIX_PATH}/lib /usr/local/lib /opt/homebrew/lib
        )
        find_library(FTXUI_COMPONENT_LIB 
            NAMES ftxui-component libftxui-component
            PATHS ${CMAKE_PREFIX_PATH}/lib /usr/local/lib /opt/homebrew/lib
        )
        
        if(FTXUI_SCREEN_LIB AND FTXUI_DOM_LIB AND FTXUI_COMPONENT_LIB)
            set(FTXUI_LIBRARIES ${FTXUI_SCREEN_LIB} ${FTXUI_DOM_LIB} ${FTXUI_COMPONENT_LIB})
        else()
            message(FATAL_ERROR "FTXUI library not found. Please install FTXUI:\n"
                                "  Ubuntu/Debian: Install from source or use vcpkg\n"
                                "  macOS: brew install ftxui\n")
        endif()
    endif()
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

# ICU for Unicode support (MVP3 requirement) - Cross-platform detection
# Try multiple methods to find ICU in order of preference
find_package(PkgConfig QUIET)

# Method 1: Try pkg-config first (works on most systems including Homebrew)
if(PkgConfig_FOUND)
    pkg_check_modules(ICU QUIET icu-uc icu-io icu-i18n)
endif()

# Method 2: Try CMake's built-in FindICU
if(NOT ICU_FOUND)
    find_package(ICU QUIET COMPONENTS uc i18n io)
endif()

# Method 3: Manual search in common locations
if(NOT ICU_FOUND)
    # Search paths for different systems
    set(ICU_SEARCH_PATHS
        # Standard system paths
        /usr/local
        /usr
        /opt/local
    )
    
    # Add dynamic homebrew paths
    file(GLOB HOMEBREW_ICU_PATHS 
         "/opt/homebrew/Cellar/icu4c*/*" 
         "/usr/local/Cellar/icu4c*/*")
    list(APPEND ICU_SEARCH_PATHS ${HOMEBREW_ICU_PATHS})
    
    # Add common Linux paths
    list(APPEND ICU_SEARCH_PATHS
        /usr/lib/x86_64-linux-gnu
        /usr/lib/aarch64-linux-gnu
    )
    
    find_path(ICU_INCLUDE_DIRS 
        NAMES unicode/unistr.h
        PATHS ${ICU_SEARCH_PATHS}
        PATH_SUFFIXES include
    )
    
    find_library(ICU_UC_LIBRARY
        NAMES icuuc libicuuc
        PATHS ${ICU_SEARCH_PATHS}
        PATH_SUFFIXES lib lib64
    )
    
    find_library(ICU_I18N_LIBRARY
        NAMES icui18n libicui18n  
        PATHS ${ICU_SEARCH_PATHS}
        PATH_SUFFIXES lib lib64
    )
    
    find_library(ICU_IO_LIBRARY
        NAMES icuio libicuio
        PATHS ${ICU_SEARCH_PATHS}
        PATH_SUFFIXES lib lib64
    )
    
    if(ICU_INCLUDE_DIRS AND ICU_UC_LIBRARY AND ICU_I18N_LIBRARY AND ICU_IO_LIBRARY)
        set(ICU_LIBRARIES ${ICU_UC_LIBRARY} ${ICU_I18N_LIBRARY} ${ICU_IO_LIBRARY})
        set(ICU_FOUND TRUE)
    endif()
endif()

# Method 4: Last resort - look for any ICU installation  
if(NOT ICU_FOUND)
    find_library(ICU_UC_LIBRARY_FALLBACK NAMES icuuc libicuuc)
    find_library(ICU_I18N_LIBRARY_FALLBACK NAMES icui18n libicui18n)
    find_library(ICU_IO_LIBRARY_FALLBACK NAMES icuio libicuio)
    find_path(ICU_INCLUDE_DIRS_FALLBACK NAMES unicode/unistr.h)
    
    if(ICU_UC_LIBRARY_FALLBACK AND ICU_I18N_LIBRARY_FALLBACK AND ICU_IO_LIBRARY_FALLBACK AND ICU_INCLUDE_DIRS_FALLBACK)
        set(ICU_LIBRARIES ${ICU_UC_LIBRARY_FALLBACK} ${ICU_I18N_LIBRARY_FALLBACK} ${ICU_IO_LIBRARY_FALLBACK})
        set(ICU_INCLUDE_DIRS ${ICU_INCLUDE_DIRS_FALLBACK})
        set(ICU_FOUND TRUE)
    endif()
endif()

# Verify ICU was found
if(NOT ICU_FOUND)
    message(FATAL_ERROR "ICU library not found. Please install ICU development packages:\n"
                        "  Ubuntu/Debian: sudo apt-get install libicu-dev\n"  
                        "  CentOS/RHEL: sudo yum install libicu-devel\n"
                        "  macOS: brew install icu4c\n"
                        "  Or set ICU_ROOT to your ICU installation directory")
endif()

message(STATUS "Found ICU libraries: ${ICU_LIBRARIES}")
message(STATUS "ICU include directories: ${ICU_INCLUDE_DIRS}")

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

# Configure version header
configure_version_header(nx)

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
    ${ICU_LIBRARIES}
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
    ${ICU_INCLUDE_DIRS}
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

# CPack configuration with semantic versioning
include(InstallRequiredSystemLibraries)
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_VERSION_MAJOR "${GIT_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${GIT_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${GIT_VERSION_PATCH}")
set(CPACK_PACKAGE_VERSION "${GIT_VERSION_FULL}")
set(CPACK_PACKAGE_CONTACT "nx-dev@example.com")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")

# Include git metadata in package
set(CPACK_PACKAGE_VENDOR "nx Project")
set(CPACK_PACKAGE_FILE_NAME "nx-${GIT_VERSION_FULL}-${CMAKE_SYSTEM_NAME}")

# Debian package
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libsqlite3-0 (>= 3.38)")
set(CPACK_DEBIAN_PACKAGE_SECTION "utils")

# RPM package
set(CPACK_RPM_PACKAGE_REQUIRES "sqlite >= 3.38")
set(CPACK_RPM_PACKAGE_GROUP "Applications/Productivity")

# Package generators
set(CPACK_GENERATOR "DEB;RPM;TGZ")

include(CPack)