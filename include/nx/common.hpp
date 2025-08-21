#pragma once

#include <expected>
#include <string>
#include <string_view>
#include <system_error>

namespace nx {

// Error handling - using std::expected pattern
enum class ErrorCode {
  kSuccess = 0,
  kInvalidArgument,
  kFileNotFound,
  kFileReadError,
  kFileWriteError,
  kFilePermissionDenied,
  kFileError,
  kDirectoryNotFound,
  kDirectoryCreateError,
  kParseError,
  kValidationError,
  kIndexError,
  kDatabaseError,
  kNetworkError,
  kEncryptionError,
  kGitError,
  kAiError,
  kConfigError,
  kExternalToolError,
  kSecurityError,
  kSystemError,
  kProcessError,
  kInvalidState,
  kNotImplemented,
  kNotFound,
  kRecoveryAttempted,
  kUnknownError
};

// Convert error code to string
std::string_view errorCodeToString(ErrorCode code);

// Error class for detailed error information
class Error {
 public:
  Error(ErrorCode code, std::string message)
      : code_(code), message_(std::move(message)) {}

  ErrorCode code() const { return code_; }
  const std::string& message() const { return message_; }

  // Create error with formatted message
  template <typename... Args>
  static Error create(ErrorCode code, const std::string& format, Args&&... args);

 private:
  ErrorCode code_;
  std::string message_;
};

// Result type alias
template <typename T>
using Result = std::expected<T, Error>;

// Convenience function for creating errors
inline Error makeError(ErrorCode code, const std::string& message) {
  return Error(code, message);
}

// Convenience function for creating error results
template<typename T>
inline Result<T> makeErrorResult(ErrorCode code, const std::string& message) {
  return std::unexpected(makeError(code, message));
}

// Version information
struct Version {
  int major;
  int minor;
  int patch;
  std::string build;

  std::string toString() const;
};

// Get version information
Version getVersion();

}  // namespace nx