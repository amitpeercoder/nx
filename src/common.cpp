#include "nx/common.hpp"

#include <sstream>

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
    case ErrorCode::kUnknownError:
      return "Unknown error";
  }
  return "Unknown error";
}

template <typename... Args>
Error Error::create(ErrorCode code, const std::string& format, Args&&... args) {
  // Simple string concatenation for C++20 compatibility
  // Note: This template is defined but not used in the current implementation
  // We use the simple constructor Error(code, message) instead
  return Error(code, format);
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
  return Version{0, 1, 0, "dev"};
}

}  // namespace nx