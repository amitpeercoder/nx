#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "nx/common.hpp"

namespace nx::util {

// Atomic filesystem operations with safety guarantees
class AtomicFileWriter {
 public:
  explicit AtomicFileWriter(const std::filesystem::path& target_path);
  ~AtomicFileWriter();

  // Non-copyable, movable
  AtomicFileWriter(const AtomicFileWriter&) = delete;
  AtomicFileWriter& operator=(const AtomicFileWriter&) = delete;
  AtomicFileWriter(AtomicFileWriter&&) = default;
  AtomicFileWriter& operator=(AtomicFileWriter&&) = default;

  // Write content to temporary file
  Result<void> write(const std::string& content);

  // Commit the changes (rename temp to target)
  Result<void> commit();

  // Cancel the operation (removes temp file)
  void cancel();

 private:
  std::filesystem::path target_path_;
  std::filesystem::path temp_path_;
  bool committed_;
  bool cancelled_;
  
  void cleanup();
};

// Secure temporary file operations
class SecureTempFile {
 public:
  // Create temporary file with O_TMPFILE if available
  static Result<SecureTempFile> create(const std::filesystem::path& dir = {});
  
  ~SecureTempFile();
  
  // Non-copyable, movable
  SecureTempFile(const SecureTempFile&) = delete;
  SecureTempFile& operator=(const SecureTempFile&) = delete;
  SecureTempFile(SecureTempFile&&) = default;
  SecureTempFile& operator=(SecureTempFile&&) = default;

  // Write content to temp file
  Result<void> write(const std::string& content);
  
  // Read content from temp file
  Result<std::string> read() const;
  
  // Get file descriptor (for encryption tools)
  int fd() const { return fd_; }
  
  // Get path (may not exist for O_TMPFILE)
  const std::filesystem::path& path() const { return path_; }

 private:
  explicit SecureTempFile(int fd, std::filesystem::path path);
  
  int fd_;
  std::filesystem::path path_;
  
  void cleanup();
};

// Filesystem utilities
class FileSystem {
 public:
  // Atomic write with fsync and rename
  static Result<void> writeFileAtomic(const std::filesystem::path& path, 
                                      const std::string& content);
  
  // Read file with error handling
  static Result<std::string> readFile(const std::filesystem::path& path);
  
  // Create directory with proper permissions
  static Result<void> createDirectories(const std::filesystem::path& path, 
                                        std::filesystem::perms perms = std::filesystem::perms::owner_all);
  
  // Move file atomically (same filesystem)
  static Result<void> moveFile(const std::filesystem::path& from, 
                               const std::filesystem::path& to);
  
  // Copy file with permissions preserved
  static Result<void> copyFile(const std::filesystem::path& from, 
                               const std::filesystem::path& to);
  
  // Check if path is safe (no symlink attacks, proper permissions)
  static Result<void> validatePath(const std::filesystem::path& path);
  
  // Get file size safely
  static Result<std::uintmax_t> fileSize(const std::filesystem::path& path);
  
  // Get last modification time
  static Result<std::filesystem::file_time_type> lastModified(const std::filesystem::path& path);
  
  // Remove file safely
  static Result<void> removeFile(const std::filesystem::path& path);
  
  // List directory contents with filtering
  static Result<std::vector<std::filesystem::path>> listDirectory(
      const std::filesystem::path& path,
      const std::string& extension_filter = "");
  
  // Ensure directory exists with XDG compliance
  static Result<void> ensureXdgDirectory(const std::filesystem::path& path);
  
  // Check available disk space
  static Result<std::uintmax_t> availableSpace(const std::filesystem::path& path);
  
  // Sync directory (ensure metadata is written)
  static Result<void> syncDirectory(const std::filesystem::path& path);

 private:
  // Internal helper for fsync
  static Result<void> fsyncFile(int fd);
  
  // Internal helper for safe file operations
  static Result<void> setFilePermissions(const std::filesystem::path& path, 
                                         std::filesystem::perms perms);
};

}  // namespace nx::util