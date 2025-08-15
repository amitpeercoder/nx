#include "nx/util/safe_process.hpp"

#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <spawn.h>
#include <signal.h>
#include <termios.h>
#include <fcntl.h>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <sstream>

extern char **environ;

namespace nx::util {

namespace {
  // Saved terminal settings for restoration
  static struct termios saved_termios;
  static bool termios_saved = false;


  /**
   * @brief Read all data from file descriptor
   */
  std::string readFromFd(int fd) {
    std::string result;
    char buffer[4096];
    ssize_t bytes_read;
    
    while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {
      result.append(buffer, bytes_read);
    }
    
    return result;
  }

  /**
   * @brief Close file descriptor safely
   */
  void safeClose(int fd) {
    if (fd >= 0) {
      close(fd);
    }
  }

  /**
   * @brief Convert vector of strings to char* array for execvp
   */
  std::vector<char*> vectorToCharArray(const std::vector<std::string>& strings) {
    std::vector<char*> result;
    result.reserve(strings.size() + 1);
    
    for (const auto& str : strings) {
      result.push_back(const_cast<char*>(str.c_str()));
    }
    result.push_back(nullptr);
    
    return result;
  }
}

Result<SafeProcess::ProcessResult> SafeProcess::execute(
    const std::string& command,
    const std::vector<std::string>& args,
    const std::optional<std::string>& working_dir) {
  return executeInternal(command, args, working_dir, true);
}

Result<std::string> SafeProcess::executeForOutput(
    const std::string& command,
    const std::vector<std::string>& args,
    const std::optional<std::string>& working_dir) {
  auto result = executeInternal(command, args, working_dir, true);
  if (!result.has_value()) {
    return std::unexpected(result.error());
  }
  
  if (!result->success()) {
    return std::unexpected(makeError(ErrorCode::kProcessError,
                                     "Command failed with exit code " + std::to_string(result->exit_code) +
                                     ": " + result->stderr_output));
  }
  
  return result->stdout_output;
}

bool SafeProcess::commandExists(const std::string& command) {
  return findCommand(command).has_value();
}

std::optional<std::string> SafeProcess::findCommand(const std::string& command) {
  // Check if command is an absolute path
  if (command.front() == '/') {
    struct stat st;
    if (stat(command.c_str(), &st) == 0 && (st.st_mode & S_IXUSR)) {
      return command;
    }
    return std::nullopt;
  }
  
  // Search in PATH
  const char* path_env = std::getenv("PATH");
  if (!path_env) {
    return std::nullopt;
  }
  
  std::string path_str(path_env);
  std::istringstream path_stream(path_str);
  std::string dir;
  
  while (std::getline(path_stream, dir, ':')) {
    if (dir.empty()) continue;
    
    std::string full_path = dir + "/" + command;
    struct stat st;
    if (stat(full_path.c_str(), &st) == 0 && (st.st_mode & S_IXUSR)) {
      return full_path;
    }
  }
  
  return std::nullopt;
}

Result<pid_t> SafeProcess::executeAsync(
    const std::string& command,
    const std::vector<std::string>& args,
    const std::optional<std::string>& working_dir) {
  
  // Find the command in PATH
  auto command_path = findCommand(command);
  if (!command_path.has_value()) {
    return std::unexpected(makeError(ErrorCode::kNotFound, 
                                     "Command not found: " + command));
  }
  
  // Prepare arguments
  std::vector<std::string> full_args;
  full_args.push_back(command);
  full_args.insert(full_args.end(), args.begin(), args.end());
  
  auto char_args = vectorToCharArray(full_args);
  
  // Change working directory if specified
  std::string old_cwd;
  if (working_dir.has_value()) {
    char* cwd = getcwd(nullptr, 0);
    if (cwd) {
      old_cwd = cwd;
      free(cwd);
    }
    
    if (chdir(working_dir->c_str()) != 0) {
      return std::unexpected(makeError(ErrorCode::kSystemError,
                                       "Failed to change directory: " + std::string(strerror(errno))));
    }
  }
  
  pid_t pid;
  int result = posix_spawn(&pid, command_path->c_str(), nullptr, nullptr, 
                          char_args.data(), environ);
  
  // Restore working directory
  if (!old_cwd.empty()) {
    chdir(old_cwd.c_str());
  }
  
  if (result != 0) {
    return std::unexpected(makeError(ErrorCode::kProcessError,
                                     "Failed to spawn process: " + std::string(strerror(result))));
  }
  
  return pid;
}

bool SafeProcess::isArgumentSafe(const std::string& arg) {
  // Check for dangerous characters that could be interpreted by shell
  const std::string dangerous_chars = "|&;(){}[]<>*?~$`\"'\\";
  
  for (char c : arg) {
    if (dangerous_chars.find(c) != std::string::npos) {
      return false;
    }
    // Check for control characters
    if (c < 32 && c != '\t' && c != '\n' && c != '\r') {
      return false;
    }
  }
  
  return true;
}

std::string SafeProcess::escapeArgument(const std::string& arg) {
  if (isArgumentSafe(arg)) {
    return arg;
  }
  
  // Simple escaping - wrap in single quotes and escape any single quotes
  std::string escaped = "'";
  for (char c : arg) {
    if (c == '\'') {
      escaped += "'\"'\"'";  // End quote, escaped quote, start quote
    } else {
      escaped += c;
    }
  }
  escaped += "'";
  
  return escaped;
}

Result<SafeProcess::ProcessResult> SafeProcess::executeInternal(
    const std::string& command,
    const std::vector<std::string>& args,
    const std::optional<std::string>& working_dir,
    bool capture_output) {
  
  // Find the command in PATH
  auto command_path = findCommand(command);
  if (!command_path.has_value()) {
    return std::unexpected(makeError(ErrorCode::kNotFound, 
                                     "Command not found: " + command));
  }
  
  // Create pipes for stdout and stderr if needed
  int stdout_pipe[2] = {-1, -1};
  int stderr_pipe[2] = {-1, -1};
  
  if (capture_output) {
    if (pipe(stdout_pipe) == -1 || pipe(stderr_pipe) == -1) {
      safeClose(stdout_pipe[0]);
      safeClose(stdout_pipe[1]);
      safeClose(stderr_pipe[0]);
      safeClose(stderr_pipe[1]);
      return std::unexpected(makeError(ErrorCode::kSystemError,
                                       "Failed to create pipes: " + std::string(strerror(errno))));
    }
  }
  
  // Prepare arguments
  std::vector<std::string> full_args;
  full_args.push_back(command);
  full_args.insert(full_args.end(), args.begin(), args.end());
  
  auto char_args = vectorToCharArray(full_args);
  
  // Set up file actions for posix_spawn
  posix_spawn_file_actions_t file_actions;
  posix_spawn_file_actions_init(&file_actions);
  
  if (capture_output) {
    // Redirect stdout and stderr to our pipes
    posix_spawn_file_actions_adddup2(&file_actions, stdout_pipe[1], STDOUT_FILENO);
    posix_spawn_file_actions_adddup2(&file_actions, stderr_pipe[1], STDERR_FILENO);
    posix_spawn_file_actions_addclose(&file_actions, stdout_pipe[0]);
    posix_spawn_file_actions_addclose(&file_actions, stderr_pipe[0]);
    posix_spawn_file_actions_addclose(&file_actions, stdout_pipe[1]);
    posix_spawn_file_actions_addclose(&file_actions, stderr_pipe[1]);
  }
  
  // Change working directory if specified
  std::string old_cwd;
  if (working_dir.has_value()) {
    char* cwd = getcwd(nullptr, 0);
    if (cwd) {
      old_cwd = cwd;
      free(cwd);
    }
    
    if (chdir(working_dir->c_str()) != 0) {
      posix_spawn_file_actions_destroy(&file_actions);
      safeClose(stdout_pipe[0]);
      safeClose(stdout_pipe[1]);
      safeClose(stderr_pipe[0]);
      safeClose(stderr_pipe[1]);
      return std::unexpected(makeError(ErrorCode::kSystemError,
                                       "Failed to change directory: " + std::string(strerror(errno))));
    }
  }
  
  pid_t pid;
  int spawn_result = posix_spawn(&pid, command_path->c_str(), &file_actions, nullptr,
                                char_args.data(), environ);
  
  posix_spawn_file_actions_destroy(&file_actions);
  
  // Restore working directory
  if (!old_cwd.empty()) {
    chdir(old_cwd.c_str());
  }
  
  if (spawn_result != 0) {
    safeClose(stdout_pipe[0]);
    safeClose(stdout_pipe[1]);
    safeClose(stderr_pipe[0]);
    safeClose(stderr_pipe[1]);
    return std::unexpected(makeError(ErrorCode::kProcessError,
                                     "Failed to spawn process: " + std::string(strerror(spawn_result))));
  }
  
  ProcessResult result;
  
  if (capture_output) {
    // Close write ends in parent
    safeClose(stdout_pipe[1]);
    safeClose(stderr_pipe[1]);
    
    // Read output
    result.stdout_output = readFromFd(stdout_pipe[0]);
    result.stderr_output = readFromFd(stderr_pipe[0]);
    
    // Close read ends
    safeClose(stdout_pipe[0]);
    safeClose(stderr_pipe[0]);
  }
  
  // Wait for process to complete
  int status;
  if (waitpid(pid, &status, 0) == -1) {
    return std::unexpected(makeError(ErrorCode::kSystemError,
                                     "Failed to wait for process: " + std::string(strerror(errno))));
  }
  
  if (WIFEXITED(status)) {
    result.exit_code = WEXITSTATUS(status);
  } else if (WIFSIGNALED(status)) {
    result.exit_code = 128 + WTERMSIG(status);
  } else {
    result.exit_code = -1;
  }
  
  return result;
}

// Terminal Control Implementation

Result<void> TerminalControl::saveSettings() {
  if (tcgetattr(STDIN_FILENO, &saved_termios) != 0) {
    return std::unexpected(makeError(ErrorCode::kSystemError,
                                     "Failed to get terminal attributes: " + std::string(strerror(errno))));
  }
  termios_saved = true;
  return {};
}

Result<void> TerminalControl::restoreSaneState() {
  struct termios sane_termios;
  
  // Get current settings
  if (tcgetattr(STDIN_FILENO, &sane_termios) != 0) {
    return std::unexpected(makeError(ErrorCode::kSystemError,
                                     "Failed to get terminal attributes: " + std::string(strerror(errno))));
  }
  
  // Set sane defaults
  sane_termios.c_iflag |= (BRKINT | ICRNL | IMAXBEL);
  sane_termios.c_iflag &= ~(IGNBRK | INLCR | IGNCR | IXOFF | INPCK | ISTRIP);
  
  sane_termios.c_oflag |= (OPOST | ONLCR);
  sane_termios.c_oflag &= ~(OCRNL | ONOCR | ONLRET);
#ifdef OLCUC
  sane_termios.c_oflag &= ~(OLCUC | OFILL);
#else
  sane_termios.c_oflag &= ~(OFILL);
#endif
  
  sane_termios.c_cflag |= (CREAD);
  sane_termios.c_cflag &= ~(CSIZE | PARENB | PARODD | CSTOPB);
  sane_termios.c_cflag |= CS8;
  
  sane_termios.c_lflag |= (ISIG | ICANON | ECHO | ECHOE | ECHOK);
#ifdef ECHOCTL
  sane_termios.c_lflag |= ECHOCTL;
#endif
#ifdef ECHOKE  
  sane_termios.c_lflag |= ECHOKE;
#endif
  sane_termios.c_lflag &= ~(NOFLSH | TOSTOP);
#ifdef XCASE
  sane_termios.c_lflag &= ~XCASE;
#endif
#ifdef ECHOPRT
  sane_termios.c_lflag &= ~ECHOPRT;
#endif
  
  // Apply settings
  if (tcsetattr(STDIN_FILENO, TCSANOW, &sane_termios) != 0) {
    return std::unexpected(makeError(ErrorCode::kSystemError,
                                     "Failed to set terminal attributes: " + std::string(strerror(errno))));
  }
  
  return {};
}

Result<void> TerminalControl::restoreSettings() {
  if (!termios_saved) {
    return std::unexpected(makeError(ErrorCode::kInvalidState,
                                     "No terminal settings saved"));
  }
  
  if (tcsetattr(STDIN_FILENO, TCSANOW, &saved_termios) != 0) {
    return std::unexpected(makeError(ErrorCode::kSystemError,
                                     "Failed to restore terminal attributes: " + std::string(strerror(errno))));
  }
  
  return {};
}

bool TerminalControl::isRawMode() {
  struct termios current_termios;
  if (tcgetattr(STDIN_FILENO, &current_termios) != 0) {
    return false;
  }
  
  // Check if canonical mode is disabled (raw mode characteristic)
  return !(current_termios.c_lflag & ICANON);
}

} // namespace nx::util