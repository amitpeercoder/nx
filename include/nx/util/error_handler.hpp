#pragma once

#include <optional>
#include <string>
#include <vector>
#include <functional>
#include <chrono>
#include "nx/common.hpp"

namespace nx::util {

// Error severity levels
enum class ErrorSeverity {
  kInfo,     // Informational messages
  kWarning,  // Recoverable issues
  kError,    // Serious errors that prevent operation
  kCritical  // Critical errors that may cause data loss
};

// Error context for providing additional debugging information
struct ErrorContext {
  std::string file_path;          // File being operated on
  std::string operation;          // Operation being performed
  std::vector<std::string> stack; // Call stack or operation stack
  std::chrono::system_clock::time_point timestamp;
  
  ErrorContext() : timestamp(std::chrono::system_clock::now()) {}
  
  ErrorContext& withFile(const std::string& path) {
    file_path = path;
    return *this;
  }
  
  ErrorContext& withOperation(const std::string& op) {
    operation = op;
    return *this;
  }
  
  ErrorContext& withStack(const std::vector<std::string>& st) {
    stack = st;
    return *this;
  }
};

// Enhanced error with context and severity
class ContextualError {
public:
  ContextualError(ErrorCode code, std::string message, ErrorSeverity severity = ErrorSeverity::kError)
    : code_(code), message_(std::move(message)), severity_(severity) {}
    
  ContextualError(ErrorCode code, std::string message, ErrorContext context, ErrorSeverity severity = ErrorSeverity::kError)
    : code_(code), message_(std::move(message)), context_(std::move(context)), severity_(severity) {}

  ErrorCode code() const { return code_; }
  const std::string& message() const { return message_; }
  const std::optional<ErrorContext>& context() const { return context_; }
  ErrorSeverity severity() const { return severity_; }
  
  // Get full error description with context
  std::string fullDescription() const;
  
  // Check if error is recoverable
  bool isRecoverable() const;
  
  // Create error with context
  static ContextualError create(ErrorCode code, const std::string& message, 
                               const ErrorContext& context, ErrorSeverity severity = ErrorSeverity::kError);

private:
  ErrorCode code_;
  std::string message_;
  std::optional<ErrorContext> context_;
  ErrorSeverity severity_;
};

// Result type with contextual error
template <typename T>
using ContextualResult = std::expected<T, ContextualError>;

// Error recovery strategy
using RecoveryStrategy = std::function<std::optional<std::string>(const ContextualError& error)>;

// Error handler for managing errors, logging, and recovery
class ErrorHandler {
public:
  static ErrorHandler& instance();
  
  // Log error and attempt recovery
  template<typename T>
  ContextualResult<T> handleError(const ContextualError& error, 
                                 const std::vector<RecoveryStrategy>& strategies = {});
  
  // Register global recovery strategy for specific error codes
  void registerRecoveryStrategy(ErrorCode code, RecoveryStrategy strategy);
  
  // Set error logging callback
  void setErrorLogger(std::function<void(const ContextualError&)> logger);
  
  // Format error for user display
  std::string formatUserError(const ContextualError& error, bool json_format = false) const;
  
  // Format error for internal logging
  std::string formatLogError(const ContextualError& error) const;
  
  // Check if we should retry operation based on error
  bool shouldRetry(const ContextualError& error, int attempt_count = 1) const;

private:
  ErrorHandler() = default;
  std::unordered_map<ErrorCode, std::vector<RecoveryStrategy>> recovery_strategies_;
  std::function<void(const ContextualError&)> error_logger_;
};

// Convenience functions for creating contextual errors
ContextualError makeContextualError(ErrorCode code, const std::string& message, 
                                  const ErrorContext& context = {}, 
                                  ErrorSeverity severity = ErrorSeverity::kError);

// Macro for creating error context at call site
#define NX_ERROR_CONTEXT() \
  ::nx::util::ErrorContext{}.withOperation(__FUNCTION__)

// Helper for file operations
#define NX_FILE_ERROR_CONTEXT(path) \
  ::nx::util::ErrorContext{}.withFile(path).withOperation(__FUNCTION__)

// Recovery strategy builders
namespace recovery {
  // Retry with exponential backoff
  RecoveryStrategy retryWithBackoff(int max_attempts = 3, int base_delay_ms = 100);
  
  // Prompt user for input
  RecoveryStrategy promptUser(const std::string& prompt);
  
  // Use fallback file path
  RecoveryStrategy fallbackPath(const std::string& fallback_path);
  
  // Create missing directory
  RecoveryStrategy createMissingDirectory();
  
  // Switch to alternative tool
  RecoveryStrategy useAlternativeTool(const std::string& alternative);
}

} // namespace nx::util