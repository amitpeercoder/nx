#pragma once

#include <string>
#include <vector>
#include <optional>
#include <memory>

#include "nx/common.hpp"

namespace nx::util {

/**
 * @brief Secure process execution utility to replace unsafe system() calls
 * 
 * This class provides safe alternatives to system(), popen(), and similar
 * functions that are vulnerable to command injection attacks.
 */
class SafeProcess {
public:
  /**
   * @brief Result of a process execution
   */
  struct ProcessResult {
    int exit_code;
    std::string stdout_output;
    std::string stderr_output;
    bool success() const { return exit_code == 0; }
  };

  /**
   * @brief Execute a command with arguments safely
   * @param command The command to execute (no shell interpretation)
   * @param args Command arguments (properly escaped)
   * @param working_dir Optional working directory
   * @return Result of execution or error
   */
  static Result<ProcessResult> execute(
    const std::string& command,
    const std::vector<std::string>& args = {},
    const std::optional<std::string>& working_dir = std::nullopt
  );

  /**
   * @brief Execute a command and return only stdout
   * @param command The command to execute
   * @param args Command arguments
   * @param working_dir Optional working directory
   * @return stdout output or error
   */
  static Result<std::string> executeForOutput(
    const std::string& command,
    const std::vector<std::string>& args = {},
    const std::optional<std::string>& working_dir = std::nullopt
  );

  /**
   * @brief Check if a command exists in PATH
   * @param command Command name to check
   * @return true if command exists and is executable
   */
  static bool commandExists(const std::string& command);

  /**
   * @brief Find the full path of a command in PATH
   * @param command Command name to find
   * @return Full path to command or nullopt if not found
   */
  static std::optional<std::string> findCommand(const std::string& command);

  /**
   * @brief Execute a command in the background
   * @param command The command to execute
   * @param args Command arguments
   * @param working_dir Optional working directory
   * @return Process ID or error
   */
  static Result<pid_t> executeAsync(
    const std::string& command,
    const std::vector<std::string>& args = {},
    const std::optional<std::string>& working_dir = std::nullopt
  );

  /**
   * @brief Validate that a string is safe for use as a command argument
   * @param arg Argument to validate
   * @return true if safe to use
   */
  static bool isArgumentSafe(const std::string& arg);

  /**
   * @brief Escape an argument for safe use in command execution
   * @param arg Argument to escape
   * @return Safely escaped argument
   */
  static std::string escapeArgument(const std::string& arg);
  
  /**
   * @brief Validate that a command name is safe for execution
   * @param command Command name to validate
   * @return true if safe to use
   */
  static bool isValidCommand(const std::string& command);
  
  /**
   * @brief Validate that an argument is safe for execution
   * @param arg Argument to validate
   * @return true if safe to use
   */
  static bool isValidArgument(const std::string& arg);

private:
  SafeProcess() = default;
  
  /**
   * @brief Internal implementation of process execution
   */
  static Result<ProcessResult> executeInternal(
    const std::string& command,
    const std::vector<std::string>& args,
    const std::optional<std::string>& working_dir,
    bool capture_output
  );
};

/**
 * @brief Terminal control utility for safe terminal operations
 * 
 * Provides safe alternatives to system("stty ...") calls
 */
class TerminalControl {
public:
  /**
   * @brief Save current terminal settings
   * @return Saved settings or error
   */
  static Result<void> saveSettings();

  /**
   * @brief Restore terminal to sane state
   * @return Success or error
   */
  static Result<void> restoreSaneState();

  /**
   * @brief Restore previously saved terminal settings
   * @return Success or error
   */
  static Result<void> restoreSettings();

  /**
   * @brief Check if terminal is in raw mode
   * @return true if in raw mode
   */
  static bool isRawMode();

private:
  TerminalControl() = default;
};

} // namespace nx::util