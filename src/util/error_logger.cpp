#include "nx/util/error_handler.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <filesystem>
#include "nx/util/xdg.hpp"

namespace nx::util {

// Error logger setup for nx application
class NxErrorLogger {
public:
  static NxErrorLogger& instance() {
    static NxErrorLogger instance_;
    return instance_;
  }
  
  void initialize() {
    if (initialized_) return;
    
    // Setup error log file path
    auto log_dir = Xdg::dataHome() / "nx" / "logs";
    std::filesystem::create_directories(log_dir);
    auto log_file = log_dir / "error.log";
    
    try {
      // Create rotating file logger for errors
      auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        log_file.string(), 1024 * 1024 * 5, 3); // 5MB files, 3 backups
      
      // Create console sink for warnings and above
      auto console_sink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
      console_sink->set_level(spdlog::level::warn);
      
      // Create multi-sink logger
      std::vector<spdlog::sink_ptr> sinks = {file_sink, console_sink};
      auto logger = std::make_shared<spdlog::logger>("nx_errors", sinks.begin(), sinks.end());
      
      // Set logging pattern with timestamp, level, and context
      logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%n] %v");
      logger->set_level(spdlog::level::debug);
      
      // Register as default logger for error handling
      spdlog::set_default_logger(logger);
      
      initialized_ = true;
      
    } catch (const std::exception& e) {
      // Fallback to console-only logging if file setup fails
      spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
      spdlog::warn("Failed to setup file logging: {}", e.what());
    }
  }
  
  void logContextualError(const ContextualError& error) {
    if (!initialized_) initialize();
    
    std::string message = formatErrorForLogging(error);
    
    switch (error.severity()) {
      case ErrorSeverity::kInfo:
        spdlog::info(message);
        break;
      case ErrorSeverity::kWarning:
        spdlog::warn(message);
        break;
      case ErrorSeverity::kError:
        spdlog::error(message);
        break;
      case ErrorSeverity::kCritical:
        spdlog::critical(message);
        break;
    }
    
    // Log additional context if available
    if (error.context()) {
      const auto& ctx = *error.context();
      if (!ctx.file_path.empty()) {
        spdlog::debug("  File: {}", ctx.file_path);
      }
      if (!ctx.operation.empty()) {
        spdlog::debug("  Operation: {}", ctx.operation);
      }
      if (!ctx.stack.empty()) {
        std::string stack_str;
        for (size_t i = 0; i < ctx.stack.size(); ++i) {
          if (i > 0) stack_str += " -> ";
          stack_str += ctx.stack[i];
        }
        spdlog::debug("  Stack: [{}]", stack_str);
      }
    }
  }

private:
  bool initialized_ = false;
  
  std::string formatErrorForLogging(const ContextualError& error) {
    return fmt::format("[{}:{}] {}", 
      static_cast<int>(error.code()),
      static_cast<int>(error.severity()),
      error.message()
    );
  }
};

// Initialize error logging when the module is loaded
void initializeErrorLogging() {
  auto& logger = NxErrorLogger::instance();
  logger.initialize();
  
  // Register error logger with the global error handler
  auto& error_handler = ErrorHandler::instance();
  error_handler.setErrorLogger([&logger](const ContextualError& error) {
    logger.logContextualError(error);
  });
}

// Helper function to be called during application startup
void setupErrorHandling() {
  initializeErrorLogging();
  
  // Register common recovery strategies
  auto& handler = ErrorHandler::instance();
  
  // File not found -> create missing directories
  handler.registerRecoveryStrategy(ErrorCode::kDirectoryNotFound, 
    recovery::createMissingDirectory());
  
  // Network errors -> retry with backoff
  handler.registerRecoveryStrategy(ErrorCode::kNetworkError, 
    recovery::retryWithBackoff(3, 1000));
  
  // External tool errors -> try alternatives
  handler.registerRecoveryStrategy(ErrorCode::kExternalToolError, 
    recovery::useAlternativeTool("fallback"));
}

} // namespace nx::util