# GitVersion.cmake - Git-based semantic versioning for nx

# Function to get git version information
function(get_git_version)
    # Initialize defaults
    set(GIT_VERSION_MAJOR 0)
    set(GIT_VERSION_MINOR 1)
    set(GIT_VERSION_PATCH 0)
    set(GIT_VERSION_TWEAK "")
    set(GIT_VERSION_BUILD "")
    set(GIT_VERSION_PRERELEASE "")
    set(GIT_VERSION_FULL "0.1.0")
    
    # Find git executable
    find_package(Git QUIET)
    if(NOT Git_FOUND)
        message(WARNING "Git not found - using default version")
        set(GIT_VERSION_FULL "0.1.0+no-git" PARENT_SCOPE)
        return()
    endif()
    
    # Get current commit hash
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_COMMIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    
    # Check if working tree is dirty
    execute_process(
        COMMAND ${GIT_EXECUTABLE} status --porcelain
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_DIRTY_CHECK
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    
    set(GIT_IS_DIRTY FALSE)
    if(NOT "${GIT_DIRTY_CHECK}" STREQUAL "")
        set(GIT_IS_DIRTY TRUE)
    endif()
    
    # Get git describe output
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --always --dirty
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_DESCRIBE
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    
    # Try to get the latest tag
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --abbrev=0
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_LATEST_TAG
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    
    # Count commits since last tag (for development builds)
    if(NOT "${GIT_LATEST_TAG}" STREQUAL "")
        execute_process(
            COMMAND ${GIT_EXECUTABLE} rev-list --count ${GIT_LATEST_TAG}..HEAD
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            OUTPUT_VARIABLE GIT_COMMITS_SINCE_TAG
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
    else()
        # Count total commits if no tags exist
        execute_process(
            COMMAND ${GIT_EXECUTABLE} rev-list --count HEAD
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            OUTPUT_VARIABLE GIT_COMMITS_SINCE_TAG
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
        set(GIT_LATEST_TAG "")
    endif()
    
    # Parse version from tag if it exists and matches semver pattern
    if(NOT "${GIT_LATEST_TAG}" STREQUAL "")
        string(REGEX MATCH "^v?([0-9]+)\\.([0-9]+)\\.([0-9]+)(-([^+]+))?(\\+(.+))?$" 
               VERSION_MATCH "${GIT_LATEST_TAG}")
        
        if(VERSION_MATCH)
            set(GIT_VERSION_MAJOR ${CMAKE_MATCH_1})
            set(GIT_VERSION_MINOR ${CMAKE_MATCH_2})
            set(GIT_VERSION_PATCH ${CMAKE_MATCH_3})
            if(CMAKE_MATCH_5)
                set(GIT_VERSION_PRERELEASE ${CMAKE_MATCH_5})
            endif()
            if(CMAKE_MATCH_7)
                set(GIT_VERSION_BUILD ${CMAKE_MATCH_7})
            endif()
        endif()
    endif()
    
    # Determine version type based on environment and build type
    set(VERSION_TYPE "unknown")
    
    # Check for CI environment variables
    if(DEFINED ENV{CI} OR DEFINED ENV{GITHUB_ACTIONS} OR DEFINED ENV{GITLAB_CI})
        set(VERSION_TYPE "ci")
    elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
        if(GIT_IS_DIRTY OR NOT "${GIT_COMMITS_SINCE_TAG}" STREQUAL "0")
            set(VERSION_TYPE "development")
        else()
            set(VERSION_TYPE "release")
        endif()
    elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(VERSION_TYPE "debug")
    else()
        set(VERSION_TYPE "development")
    endif()
    
    # Build version string based on type
    set(BASE_VERSION "${GIT_VERSION_MAJOR}.${GIT_VERSION_MINOR}.${GIT_VERSION_PATCH}")
    
    if(VERSION_TYPE STREQUAL "release")
        # Clean release: 1.0.0
        set(GIT_VERSION_FULL "${BASE_VERSION}")
        if(NOT "${GIT_VERSION_PRERELEASE}" STREQUAL "")
            set(GIT_VERSION_FULL "${BASE_VERSION}-${GIT_VERSION_PRERELEASE}")
        endif()
    elseif(VERSION_TYPE STREQUAL "development")
        # Development build: 1.0.0-dev.123.abc1234 or 1.0.0-dev.abc1234
        if(NOT "${GIT_COMMITS_SINCE_TAG}" STREQUAL "0" AND NOT "${GIT_COMMITS_SINCE_TAG}" STREQUAL "")
            set(GIT_VERSION_FULL "${BASE_VERSION}-dev.${GIT_COMMITS_SINCE_TAG}.${GIT_COMMIT_HASH}")
        else()
            set(GIT_VERSION_FULL "${BASE_VERSION}-dev.${GIT_COMMIT_HASH}")
        endif()
        if(GIT_IS_DIRTY)
            set(GIT_VERSION_FULL "${GIT_VERSION_FULL}+dirty")
        endif()
    elseif(VERSION_TYPE STREQUAL "debug")
        # Debug build: 1.0.0-debug.abc1234
        set(GIT_VERSION_FULL "${BASE_VERSION}-debug.${GIT_COMMIT_HASH}")
        if(GIT_IS_DIRTY)
            set(GIT_VERSION_FULL "${GIT_VERSION_FULL}+dirty")
        endif()
    elseif(VERSION_TYPE STREQUAL "ci")
        # CI build: 1.0.0-ci.123.abc1234
        if(NOT "${GIT_COMMITS_SINCE_TAG}" STREQUAL "0" AND NOT "${GIT_COMMITS_SINCE_TAG}" STREQUAL "")
            set(GIT_VERSION_FULL "${BASE_VERSION}-ci.${GIT_COMMITS_SINCE_TAG}.${GIT_COMMIT_HASH}")
        else()
            set(GIT_VERSION_FULL "${BASE_VERSION}-ci.${GIT_COMMIT_HASH}")
        endif()
    else()
        # Fallback: 1.0.0-unknown.abc1234
        set(GIT_VERSION_FULL "${BASE_VERSION}-unknown.${GIT_COMMIT_HASH}")
    endif()
    
    # Convert boolean to C++ boolean literal
    if(GIT_IS_DIRTY)
        set(GIT_IS_DIRTY_BOOL "true")
    else()
        set(GIT_IS_DIRTY_BOOL "false")
    endif()
    
    # Set parent scope variables
    set(GIT_VERSION_MAJOR ${GIT_VERSION_MAJOR} PARENT_SCOPE)
    set(GIT_VERSION_MINOR ${GIT_VERSION_MINOR} PARENT_SCOPE)
    set(GIT_VERSION_PATCH ${GIT_VERSION_PATCH} PARENT_SCOPE)
    set(GIT_VERSION_PRERELEASE ${GIT_VERSION_PRERELEASE} PARENT_SCOPE)
    set(GIT_VERSION_BUILD ${GIT_VERSION_BUILD} PARENT_SCOPE)
    set(GIT_VERSION_FULL ${GIT_VERSION_FULL} PARENT_SCOPE)
    set(GIT_COMMIT_HASH ${GIT_COMMIT_HASH} PARENT_SCOPE)
    set(GIT_IS_DIRTY ${GIT_IS_DIRTY} PARENT_SCOPE)
    set(GIT_IS_DIRTY_BOOL ${GIT_IS_DIRTY_BOOL} PARENT_SCOPE)
    set(GIT_COMMITS_SINCE_TAG ${GIT_COMMITS_SINCE_TAG} PARENT_SCOPE)
    set(VERSION_TYPE ${VERSION_TYPE} PARENT_SCOPE)
    
    message(STATUS "Git version info:")
    message(STATUS "  Version: ${GIT_VERSION_FULL}")
    message(STATUS "  Type: ${VERSION_TYPE}")
    message(STATUS "  Commit: ${GIT_COMMIT_HASH}")
    message(STATUS "  Dirty: ${GIT_IS_DIRTY}")
    if(NOT "${GIT_COMMITS_SINCE_TAG}" STREQUAL "")
        message(STATUS "  Commits since tag: ${GIT_COMMITS_SINCE_TAG}")
    endif()
endfunction()

# Function to configure version header
function(configure_version_header TARGET_NAME)
    # Generate version.hpp with git information
    configure_file(
        ${CMAKE_SOURCE_DIR}/cmake/version.hpp.in
        ${CMAKE_BINARY_DIR}/include/nx/version.hpp
        @ONLY
    )
    
    # Add binary include directory to target
    target_include_directories(${TARGET_NAME} PRIVATE ${CMAKE_BINARY_DIR}/include)
endfunction()