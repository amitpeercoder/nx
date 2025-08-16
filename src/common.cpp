#include "nx/common.hpp"

#include <sstream>

#if __cpp_lib_format >= 201907L
#include <format>
#endif

#ifdef __has_include
  #if __has_include("nx/version.hpp")
    #include "nx/version.hpp"
    #define HAS_GIT_VERSION 1
  #endif
#endif

namespace nx {

std::string_view errorCodeToString(ErrorCode code) {
  switch (code) {
    case ErrorCode::kSuccess:
      return "Success";
    case ErrorCode::kInvalidArgument:
      return "Invalid argument";
    case ErrorCode::kFileNotFound:
      return "File not found";
    case ErrorCode::kFileReadError:
      return "File read error";
    case ErrorCode::kFileWriteError:
      return "File write error";
    case ErrorCode::kFilePermissionDenied:
      return "File permission denied";
    case ErrorCode::kFileError:
      return "File error";
    case ErrorCode::kDirectoryNotFound:
      return "Directory not found";
    case ErrorCode::kDirectoryCreateError:
      return "Directory create error";
    case ErrorCode::kParseError:
      return "Parse error";
    case ErrorCode::kValidationError:
      return "Validation error";
    case ErrorCode::kIndexError:
      return "Index error";
    case ErrorCode::kDatabaseError:
      return "Database error";
    case ErrorCode::kNetworkError:
      return "Network error";
    case ErrorCode::kEncryptionError:
      return "Encryption error";
    case ErrorCode::kGitError:
      return "Git error";
    case ErrorCode::kConfigError:
      return "Configuration error";
    case ErrorCode::kExternalToolError:
      return "External tool error";
    case ErrorCode::kSecurityError:
      return "Security error";
    case ErrorCode::kSystemError:
      return "System error";
    case ErrorCode::kProcessError:
      return "Process error";
    case ErrorCode::kInvalidState:
      return "Invalid state";
    case ErrorCode::kNotImplemented:
      return "Not implemented";
    case ErrorCode::kNotFound:
      return "Not found";
    case ErrorCode::kRecoveryAttempted:
      return "Recovery attempted";
    case ErrorCode::kUnknownError:
      return "Unknown error";
  }
  return "Unknown error";
}

template <typename... Args>
Error Error::create(ErrorCode code, const std::string& format, Args&&... args) {
  // Use std::format when available (C++20), otherwise fallback to simple formatting
  #if __cpp_lib_format >= 201907L
    return Error(code, std::vformat(format, std::make_format_args(args...)));
  #else
    // Fallback implementation using stringstream
    std::ostringstream oss;
    oss << format;
    ((oss << " " << args), ...); // C++17 fold expression
    return Error(code, oss.str());
  #endif
}

std::string Version::toString() const {
  std::ostringstream oss;
  oss << major << "." << minor << "." << patch;
  if (!build.empty()) {
    oss << "+" << build;
  }
  return oss.str();
}

Version getVersion() {
#ifdef HAS_GIT_VERSION
  // Use git-based version information
  Version version;
  version.major = GitVersionInfo::major;
  version.minor = GitVersionInfo::minor;
  version.patch = GitVersionInfo::patch;
  
  // Construct build string from git info
  std::string build_info;
  
  // Add prerelease if present
  if (!GitVersionInfo::prerelease.empty()) {
    build_info = std::string(GitVersionInfo::prerelease);
  }
  
  // Add build metadata
  if (!GitVersionInfo::build.empty()) {
    if (!build_info.empty()) {
      build_info += "+";
    }
    build_info += std::string(GitVersionInfo::build);
  }
  
  // For development builds, construct from git info
  if (GitVersionInfo::isDevelopment()) {
    build_info = "";
    if (GitVersionInfo::version_type == "debug") {
      build_info = "debug.";
    } else {
      build_info = "dev.";
    }
    
    // Add commit count if available
    if (!GitVersionInfo::commits_since_tag.empty() && 
        GitVersionInfo::commits_since_tag != "0") {
      build_info += std::string(GitVersionInfo::commits_since_tag) + ".";
    }
    
    // Add commit hash
    build_info += std::string(GitVersionInfo::commit_hash);
    
    // Add dirty flag
    if (GitVersionInfo::isDirty()) {
      build_info += "+dirty";
    }
  }
  
  version.build = build_info;
  return version;
#else
  // Fallback when git version info is not available
  return Version{0, 1, 0, "no-git"};
#endif
}

}  // namespace nx