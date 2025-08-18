#pragma once

#include <iostream>
#include <string>
#include "nx/util/error_handler.hpp"
#include "nx/cli/application.hpp"

namespace nx::cli {

// Command-specific error handler that formats errors for CLI output
class CommandErrorHandler {
public:
  explicit CommandErrorHandler(const GlobalOptions& options) : options_(options) {}
  
  // Handle and display command errors
  int handleCommandError(const util::ContextualError& error);
  
  // Handle and display legacy errors (for gradual migration)
  int handleLegacyError(const Error& error, const std::string& operation = "");
  
  // Convert legacy error to contextual error
  util::ContextualError convertLegacyError(const Error& error, const std::string& operation = "");
  
  // Display success message
  void displaySuccess(const std::string& message);
  
  // Display warning
  void displayWarning(const std::string& message);
  
  // Display info message
  void displayInfo(const std::string& message);

private:
  const GlobalOptions& options_;
  
  void logError(const util::ContextualError& error);
  bool shouldShowStackTrace() const;
};

// Helper macros for command error handling
#define NX_HANDLE_ERROR(handler, error) \
  return (handler).handleCommandError(error)

#define NX_HANDLE_LEGACY_ERROR(handler, error, operation) \
  return (handler).handleLegacyError(error, operation)

#define NX_TRY_COMMAND(handler, result, operation) \
  do { \
    if (!(result).has_value()) { \
      auto ctx_error = (handler).convertLegacyError((result).error(), operation); \
      return (handler).handleCommandError(ctx_error); \
    } \
  } while(0)

// Enhanced result handling with automatic error conversion
template<typename T>
class CommandResult {
public:
  CommandResult(Result<T> result, CommandErrorHandler& handler, const std::string& operation = "")
    : result_(std::move(result)), handler_(handler), operation_(operation) {}
  
  // Implicit conversion to int for command return values
  operator int() {
    if (result_.has_value()) {
      return 0; // Success
    }
    auto ctx_error = handler_.convertLegacyError(result_.error(), operation_);
    return handler_.handleCommandError(ctx_error);
  }
  
  // Check if successful
  bool hasValue() const { return result_.has_value(); }
  
  // Get value (throws if error)
  T& value() { return result_.value(); }
  const T& value() const { return result_.value(); }
  
  // Get error
  const Error& error() const { return result_.error(); }

private:
  Result<T> result_;
  CommandErrorHandler& handler_;
  std::string operation_;
};

// Factory function for creating command results
template<typename T>
CommandResult<T> makeCommandResult(Result<T> result, CommandErrorHandler& handler, 
                                  const std::string& operation = "") {
  return CommandResult<T>(std::move(result), handler, operation);
}

} // namespace nx::cli