#include "nx/cli/command_error_handler.hpp"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include "nx/util/filesystem.hpp"

namespace nx::cli {

int CommandErrorHandler::handleCommandError(const util::ContextualError& error) {
  // Log the error for debugging
  logError(error);
  
  // Display user-friendly error message
  auto& handler = util::ErrorHandler::instance();
  std::string formatted_error = handler.formatUserError(error, options_.json);
  
  if (options_.json) {
    std::cout << formatted_error << std::endl;
  } else {
    std::cerr << formatted_error << std::endl;
    
    // Show stack trace in verbose mode
    if (shouldShowStackTrace() && error.context() && !error.context()->stack.empty()) {
      std::cerr << "\nCall stack:" << std::endl;
      for (const auto& frame : error.context()->stack) {
        std::cerr << "  " << frame << std::endl;
      }
    }
  }
  
  // Return appropriate exit code based on error severity
  switch (error.severity()) {
    case util::ErrorSeverity::kInfo:
    case util::ErrorSeverity::kWarning:
      return 0; // Don't fail for warnings/info
    case util::ErrorSeverity::kError:
      return 1; // Standard error exit code
    case util::ErrorSeverity::kCritical:
      return 2; // Critical error exit code
  }
  
  return 1; // Default error exit code
}

int CommandErrorHandler::handleLegacyError(const Error& error, const std::string& operation) {
  auto ctx_error = convertLegacyError(error, operation);
  return handleCommandError(ctx_error);
}

util::ContextualError CommandErrorHandler::convertLegacyError(const Error& error, const std::string& operation) {
  util::ErrorContext context;
  if (!operation.empty()) {
    context.withOperation(operation);
  }
  
  // Determine severity based on error code
  util::ErrorSeverity severity = util::ErrorSeverity::kError;
  switch (error.code()) {
    case ErrorCode::kNotFound:
    case ErrorCode::kValidationError:
      severity = util::ErrorSeverity::kWarning;
      break;
    case ErrorCode::kFilePermissionDenied:
    case ErrorCode::kDatabaseError:
    case ErrorCode::kEncryptionError:
      severity = util::ErrorSeverity::kCritical;
      break;
    default:
      severity = util::ErrorSeverity::kError;
      break;
  }
  
  return util::ContextualError(error.code(), error.message(), context, severity);
}

void CommandErrorHandler::displaySuccess(const std::string& message) {
  if (options_.json) {
    nlohmann::json success_json;
    success_json["success"] = true;
    success_json["message"] = message;
    std::cout << success_json.dump() << std::endl;
  } else {
    std::cout << "\033[32m✓\033[0m " << message << std::endl;
  }
}

void CommandErrorHandler::displayWarning(const std::string& message) {
  if (options_.json) {
    nlohmann::json warning_json;
    warning_json["warning"] = true;
    warning_json["message"] = message;
    std::cout << warning_json.dump() << std::endl;
  } else {
    std::cerr << "\033[33m⚠\033[0m " << message << std::endl;
  }
}

void CommandErrorHandler::displayInfo(const std::string& message) {
  if (options_.json) {
    nlohmann::json info_json;
    info_json["info"] = true;
    info_json["message"] = message;
    std::cout << info_json.dump() << std::endl;
  } else {
    std::cout << "\033[36mℹ\033[0m " << message << std::endl;
  }
}

void CommandErrorHandler::logError(const util::ContextualError& error) {
  // Only log to file if not in JSON mode (to avoid mixing output)
  if (!options_.json) {
    auto& handler = util::ErrorHandler::instance();
    std::string log_message = handler.formatLogError(error);
    
    // Log using spdlog
    switch (error.severity()) {
      case util::ErrorSeverity::kInfo:
        spdlog::info(log_message);
        break;
      case util::ErrorSeverity::kWarning:
        spdlog::warn(log_message);
        break;
      case util::ErrorSeverity::kError:
        spdlog::error(log_message);
        break;
      case util::ErrorSeverity::kCritical:
        spdlog::critical(log_message);
        break;
    }
  }
}

bool CommandErrorHandler::shouldShowStackTrace() const {
  return options_.verbose > 1; // Show stack trace in very verbose mode (-vv)
}

} // namespace nx::cli