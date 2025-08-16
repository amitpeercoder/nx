#include "nx/util/error_handler.hpp"

#include <iostream>
#include <sstream>
#include <iomanip>
#include <thread>
#include <filesystem>
#include <nlohmann/json.hpp>
#include "nx/util/filesystem.hpp"

namespace nx::util {

std::string ContextualError::fullDescription() const {
  std::ostringstream oss;
  oss << errorCodeToString(code_) << ": " << message_;
  
  if (context_) {
    if (!context_->operation.empty()) {
      oss << " (during " << context_->operation << ")";
    }
    if (!context_->file_path.empty()) {
      oss << " [file: " << context_->file_path << "]";
    }
    if (!context_->stack.empty()) {
      oss << " [stack: ";
      for (size_t i = 0; i < context_->stack.size(); ++i) {
        if (i > 0) oss << " -> ";
        oss << context_->stack[i];
      }
      oss << "]";
    }
  }
  
  return oss.str();
}

bool ContextualError::isRecoverable() const {
  switch (code_) {
    case ErrorCode::kFileNotFound:
    case ErrorCode::kDirectoryNotFound:
    case ErrorCode::kFilePermissionDenied:
    case ErrorCode::kNetworkError:
    case ErrorCode::kExternalToolError:
      return true;
    case ErrorCode::kFileReadError:
    case ErrorCode::kFileWriteError:
    case ErrorCode::kIndexError:
    case ErrorCode::kDatabaseError:
      return severity_ != ErrorSeverity::kCritical;
    default:
      return severity_ == ErrorSeverity::kWarning || severity_ == ErrorSeverity::kInfo;
  }
}

ContextualError ContextualError::create(ErrorCode code, const std::string& message, 
                                       const ErrorContext& context, ErrorSeverity severity) {
  return ContextualError(code, message, context, severity);
}

ErrorHandler& ErrorHandler::instance() {
  static ErrorHandler instance_;
  return instance_;
}

template<typename T>
ContextualResult<T> ErrorHandler::handleError(const ContextualError& error, 
                                             const std::vector<RecoveryStrategy>& strategies) {
  // Log the error
  if (error_logger_) {
    error_logger_(error);
  }
  
  // Try recovery strategies
  std::vector<RecoveryStrategy> all_strategies = strategies;
  
  // Add global strategies for this error code
  auto global_it = recovery_strategies_.find(error.code());
  if (global_it != recovery_strategies_.end()) {
    all_strategies.insert(all_strategies.end(), 
                         global_it->second.begin(), global_it->second.end());
  }
  
  for (const auto& strategy : all_strategies) {
    auto recovery_result = strategy(error);
    if (recovery_result.has_value()) {
      // Recovery succeeded, but we can't reconstruct T from just a string
      // This would need to be implemented per operation type
      // For now, just return the error
      break;
    }
  }
  
  return std::unexpected(error);
}

void ErrorHandler::registerRecoveryStrategy(ErrorCode code, RecoveryStrategy strategy) {
  recovery_strategies_[code].push_back(std::move(strategy));
}

void ErrorHandler::setErrorLogger(std::function<void(const ContextualError&)> logger) {
  error_logger_ = std::move(logger);
}

std::string ErrorHandler::formatUserError(const ContextualError& error, bool json_format) const {
  if (json_format) {
    nlohmann::json error_json;
    error_json["error"] = true;
    error_json["code"] = static_cast<int>(error.code());
    error_json["message"] = error.message();
    error_json["severity"] = static_cast<int>(error.severity());
    
    if (error.context()) {
      auto& ctx = *error.context();
      if (!ctx.file_path.empty()) {
        error_json["file"] = ctx.file_path;
      }
      if (!ctx.operation.empty()) {
        error_json["operation"] = ctx.operation;
      }
      if (!ctx.stack.empty()) {
        error_json["stack"] = ctx.stack;
      }
    }
    
    return error_json.dump();
  } else {
    std::ostringstream oss;
    
    // Color coding based on severity
    const char* color_code = "";
    const char* severity_text = "";
    const char* reset_code = "\033[0m";
    
    switch (error.severity()) {
      case ErrorSeverity::kInfo:
        color_code = "\033[36m"; // Cyan
        severity_text = "Info";
        break;
      case ErrorSeverity::kWarning:
        color_code = "\033[33m"; // Yellow
        severity_text = "Warning";
        break;
      case ErrorSeverity::kError:
        color_code = "\033[31m"; // Red
        severity_text = "Error";
        break;
      case ErrorSeverity::kCritical:
        color_code = "\033[35m"; // Magenta
        severity_text = "Critical";
        break;
    }
    
    oss << color_code << severity_text << reset_code << ": " << error.message();
    
    if (error.context()) {
      const auto& ctx = *error.context();
      if (!ctx.file_path.empty()) {
        oss << "\n  File: " << ctx.file_path;
      }
      if (!ctx.operation.empty()) {
        oss << "\n  Operation: " << ctx.operation;
      }
    }
    
    // Add helpful suggestions based on error type
    switch (error.code()) {
      case ErrorCode::kFileNotFound:
        oss << "\n  Suggestion: Check if the file path is correct and the file exists";
        break;
      case ErrorCode::kFilePermissionDenied:
        oss << "\n  Suggestion: Check file permissions or run with appropriate privileges";
        break;
      case ErrorCode::kDirectoryNotFound:
        oss << "\n  Suggestion: Create the directory first or check the path";
        break;
      case ErrorCode::kExternalToolError:
        oss << "\n  Suggestion: Ensure the required external tool is installed and in PATH";
        break;
      case ErrorCode::kNetworkError:
        oss << "\n  Suggestion: Check network connectivity and try again";
        break;
      default:
        break;
    }
    
    return oss.str();
  }
}

std::string ErrorHandler::formatLogError(const ContextualError& error) const {
  std::ostringstream oss;
  
  // Timestamp
  auto time_t = std::chrono::system_clock::to_time_t(
    error.context() ? error.context()->timestamp : std::chrono::system_clock::now());
  oss << "[" << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S") << "] ";
  
  // Severity level
  switch (error.severity()) {
    case ErrorSeverity::kInfo: oss << "INFO"; break;
    case ErrorSeverity::kWarning: oss << "WARN"; break;
    case ErrorSeverity::kError: oss << "ERROR"; break;
    case ErrorSeverity::kCritical: oss << "CRITICAL"; break;
  }
  
  oss << " [" << static_cast<int>(error.code()) << "] " << error.fullDescription();
  
  return oss.str();
}

bool ErrorHandler::shouldRetry(const ContextualError& error, int attempt_count) const {
  if (attempt_count >= 3) return false;
  
  switch (error.code()) {
    case ErrorCode::kNetworkError:
    case ErrorCode::kFileWriteError:  // Might be temporary lock
    case ErrorCode::kExternalToolError:  // Tool might be temporarily unavailable
      return error.severity() != ErrorSeverity::kCritical;
    default:
      return false;
  }
}

ContextualError makeContextualError(ErrorCode code, const std::string& message, 
                                  const ErrorContext& context, ErrorSeverity severity) {
  return ContextualError::create(code, message, context, severity);
}

namespace recovery {

RecoveryStrategy retryWithBackoff(int max_attempts, int base_delay_ms) {
  return [max_attempts, base_delay_ms](const ContextualError& error) -> std::optional<std::string> {
    static thread_local std::unordered_map<std::string, int> attempt_counts;
    
    std::string key = std::to_string(static_cast<int>(error.code())) + ":" + error.message();
    int attempts = ++attempt_counts[key];
    
    if (attempts >= max_attempts) {
      attempt_counts.erase(key);
      return std::nullopt;
    }
    
    // Exponential backoff
    int delay_ms = base_delay_ms * (1 << (attempts - 1));
    std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
    
    return "Retried after " + std::to_string(delay_ms) + "ms";
  };
}

RecoveryStrategy promptUser(const std::string& prompt) {
  return [prompt](const ContextualError& error) -> std::optional<std::string> {
    std::cout << prompt << " [y/N]: ";
    std::string response;
    std::getline(std::cin, response);
    
    if (response == "y" || response == "Y" || response == "yes" || response == "Yes") {
      return "User chose to continue";
    }
    
    return std::nullopt;
  };
}

RecoveryStrategy fallbackPath(const std::string& fallback_path) {
  return [fallback_path](const ContextualError& error) -> std::optional<std::string> {
    if (error.code() == ErrorCode::kFileNotFound || error.code() == ErrorCode::kDirectoryNotFound) {
      if (std::filesystem::exists(fallback_path)) {
        return "Using fallback path: " + fallback_path;
      }
    }
    return std::nullopt;
  };
}

RecoveryStrategy createMissingDirectory() {
  return [](const ContextualError& error) -> std::optional<std::string> {
    if (error.code() == ErrorCode::kDirectoryNotFound && error.context()) {
      const auto& ctx = *error.context();
      if (!ctx.file_path.empty()) {
        auto dir_path = std::filesystem::path(ctx.file_path).parent_path();
        auto result = FileSystem::createDirectories(dir_path);
        if (result.has_value()) {
          return "Created missing directory: " + dir_path.string();
        }
      }
    }
    return std::nullopt;
  };
}

RecoveryStrategy useAlternativeTool(const std::string& alternative) {
  return [alternative](const ContextualError& error) -> std::optional<std::string> {
    if (error.code() == ErrorCode::kExternalToolError) {
      // This would need integration with the specific tool usage context
      return "Attempting to use alternative tool: " + alternative;
    }
    return std::nullopt;
  };
}

} // namespace recovery

} // namespace nx::util