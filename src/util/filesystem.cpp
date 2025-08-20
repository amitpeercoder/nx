#include "nx/util/filesystem.hpp"

#include <fstream>
#include <random>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
#else
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#endif

namespace nx::util {

// AtomicFileWriter implementation
AtomicFileWriter::AtomicFileWriter(const std::filesystem::path& target_path)
    : target_path_(target_path), committed_(false), cancelled_(false) {
  // Generate unique temporary filename
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(100000, 999999);
  
  temp_path_ = target_path_;
  temp_path_ += ".tmp." + std::to_string(dis(gen));
}

AtomicFileWriter::~AtomicFileWriter() {
  if (!committed_ && !cancelled_) {
    cleanup();
  }
}

Result<void> AtomicFileWriter::write(const std::string& content) {
  if (committed_ || cancelled_) {
    return std::unexpected(makeError(ErrorCode::kFileWriteError, "Writer already used"));
  }
  
  std::ofstream file(temp_path_, std::ios::binary);
  if (!file) {
    return std::unexpected(makeError(ErrorCode::kFileWriteError, 
                                     "Cannot create temporary file: " + temp_path_.string()));
  }
  
  file.write(content.data(), content.size());
  if (!file) {
    cleanup();
    return std::unexpected(makeError(ErrorCode::kFileWriteError, 
                                     "Failed to write to temporary file"));
  }
  
  file.close();
  if (!file) {
    cleanup();
    return std::unexpected(makeError(ErrorCode::kFileWriteError, 
                                     "Failed to close temporary file"));
  }
  
  return {};
}

Result<void> AtomicFileWriter::commit() {
  if (committed_) {
    return std::unexpected(makeError(ErrorCode::kFileWriteError, "Already committed"));
  }
  if (cancelled_) {
    return std::unexpected(makeError(ErrorCode::kFileWriteError, "Operation cancelled"));
  }
  
  // Ensure parent directory exists
  auto parent = target_path_.parent_path();
  if (!parent.empty() && !std::filesystem::exists(parent)) {
    std::error_code ec;
    std::filesystem::create_directories(parent, ec);
    if (ec) {
      cleanup();
      return std::unexpected(makeError(ErrorCode::kDirectoryCreateError, 
                                       "Cannot create parent directory: " + ec.message()));
    }
  }
  
  // Sync the temporary file
#ifdef _WIN32
  int fd = _open(temp_path_.string().c_str(), _O_RDONLY);
  if (fd >= 0) {
    _commit(fd);
    _close(fd);
  }
#else
  int fd = open(temp_path_.c_str(), O_RDONLY);
  if (fd >= 0) {
    fsync(fd);
    close(fd);
  }
#endif
  
  // Atomic rename
  std::error_code ec;
  std::filesystem::rename(temp_path_, target_path_, ec);
  if (ec) {
    cleanup();
    return std::unexpected(makeError(ErrorCode::kFileWriteError, 
                                     "Atomic rename failed: " + ec.message()));
  }
  
  // Sync parent directory to ensure rename is persistent
#ifdef _WIN32
  if (!parent.empty()) {
    int dir_fd = _open(parent.string().c_str(), _O_RDONLY);
    if (dir_fd >= 0) {
      _commit(dir_fd);
      _close(dir_fd);
    }
  }
#else
  if (!parent.empty()) {
    int dir_fd = open(parent.c_str(), O_RDONLY);
    if (dir_fd >= 0) {
      fsync(dir_fd);
      close(dir_fd);
    }
  }
#endif
  
  committed_ = true;
  return {};
}

void AtomicFileWriter::cancel() {
  if (!committed_) {
    cancelled_ = true;
    cleanup();
  }
}

void AtomicFileWriter::cleanup() {
  if (std::filesystem::exists(temp_path_)) {
    std::filesystem::remove(temp_path_);
  }
}

// SecureTempFile implementation
Result<SecureTempFile> SecureTempFile::create(const std::filesystem::path& dir) {
  std::filesystem::path temp_dir = dir.empty() ? std::filesystem::temp_directory_path() : dir;
  
  // Try O_TMPFILE first (Linux-specific)
#if defined(O_TMPFILE) && !defined(_WIN32)
  int fd = open(temp_dir.c_str(), O_TMPFILE | O_RDWR, S_IRUSR | S_IWUSR);
  if (fd >= 0) {
    return SecureTempFile(fd, temp_dir / "<anonymous>");
  }
#endif
  
  // Fallback to regular temporary file
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(100000, 999999);
  
  std::filesystem::path temp_path = temp_dir / ("nx_temp." + std::to_string(dis(gen)));
  
#ifdef _WIN32
  int fd = _open(temp_path.string().c_str(), _O_CREAT | _O_RDWR | _O_EXCL, _S_IREAD | _S_IWRITE);
#else
  int fd = open(temp_path.c_str(), O_CREAT | O_RDWR | O_EXCL, S_IRUSR | S_IWUSR);
#endif
  if (fd < 0) {
    return std::unexpected(makeError(ErrorCode::kFileWriteError, 
                                     "Cannot create secure temporary file"));
  }
  
  return SecureTempFile(fd, temp_path);
}

SecureTempFile::SecureTempFile(int fd, std::filesystem::path path)
    : fd_(fd), path_(std::move(path)) {}

SecureTempFile::~SecureTempFile() {
  cleanup();
}

Result<void> SecureTempFile::write(const std::string& content) {
  if (fd_ < 0) {
    return std::unexpected(makeError(ErrorCode::kFileWriteError, "File not open"));
  }
  
#ifdef _WIN32
  int written = _write(fd_, content.data(), static_cast<unsigned int>(content.size()));
  if (written < 0 || static_cast<size_t>(written) != content.size()) {
    return std::unexpected(makeError(ErrorCode::kFileWriteError, "Write failed"));
  }
  
  if (_commit(fd_) < 0) {
    return std::unexpected(makeError(ErrorCode::kFileWriteError, "Sync failed"));
  }
#else
  ssize_t written = ::write(fd_, content.data(), content.size());
  if (written < 0 || static_cast<size_t>(written) != content.size()) {
    return std::unexpected(makeError(ErrorCode::kFileWriteError, "Write failed"));
  }
  
  if (fsync(fd_) < 0) {
    return std::unexpected(makeError(ErrorCode::kFileWriteError, "Sync failed"));
  }
#endif
  
  return {};
}

Result<std::string> SecureTempFile::read() const {
  if (fd_ < 0) {
    return std::unexpected(makeError(ErrorCode::kFileReadError, "File not open"));
  }
  
  // Get file size
#ifdef _WIN32
  long size = _lseek(fd_, 0, SEEK_END);
  if (size < 0) {
    return std::unexpected(makeError(ErrorCode::kFileReadError, "Cannot get file size"));
  }
  
  if (_lseek(fd_, 0, SEEK_SET) < 0) {
    return std::unexpected(makeError(ErrorCode::kFileReadError, "Cannot seek to beginning"));
  }
  
  std::string content(size, '\0');
  int bytes_read = _read(fd_, content.data(), static_cast<unsigned int>(size));
  if (bytes_read != size) {
    return std::unexpected(makeError(ErrorCode::kFileReadError, "Read failed"));
#else
  off_t size = lseek(fd_, 0, SEEK_END);
  if (size < 0) {
    return std::unexpected(makeError(ErrorCode::kFileReadError, "Cannot get file size"));
  }
  
  if (lseek(fd_, 0, SEEK_SET) < 0) {
    return std::unexpected(makeError(ErrorCode::kFileReadError, "Cannot seek to beginning"));
  }
  
  std::string content(size, '\0');
  ssize_t bytes_read = ::read(fd_, content.data(), size);
  if (bytes_read != size) {
    return std::unexpected(makeError(ErrorCode::kFileReadError, "Read failed"));
#endif
  }
  
  return content;
}

void SecureTempFile::cleanup() {
  if (fd_ >= 0) {
#ifdef _WIN32
    _close(fd_);
#else
    close(fd_);
#endif
    fd_ = -1;
  }
  
  // Remove file if it's not O_TMPFILE
  if (path_.filename() != "<anonymous>" && std::filesystem::exists(path_)) {
    std::filesystem::remove(path_);
  }
}

// FileSystem implementation
Result<void> FileSystem::writeFileAtomic(const std::filesystem::path& path, 
                                         const std::string& content) {
  AtomicFileWriter writer(path);
  
  auto write_result = writer.write(content);
  if (!write_result.has_value()) {
    return write_result;
  }
  
  return writer.commit();
}

Result<std::string> FileSystem::readFile(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return std::unexpected(makeError(ErrorCode::kFileNotFound, 
                                     "Cannot open file: " + path.string()));
  }
  
  file.seekg(0, std::ios::end);
  auto size = file.tellg();
  if (size < 0) {
    return std::unexpected(makeError(ErrorCode::kFileReadError, "Cannot get file size"));
  }
  
  file.seekg(0, std::ios::beg);
  
  std::string content(size, '\0');
  file.read(content.data(), size);
  
  if (!file) {
    return std::unexpected(makeError(ErrorCode::kFileReadError, "Read failed"));
  }
  
  return content;
}

Result<void> FileSystem::createDirectories(const std::filesystem::path& path, 
                                           std::filesystem::perms perms) {
  std::error_code ec;
  
  if (!std::filesystem::create_directories(path, ec) && ec) {
    return std::unexpected(makeError(ErrorCode::kDirectoryCreateError, 
                                     "Cannot create directories: " + ec.message()));
  }
  
  // Set permissions
  std::filesystem::permissions(path, perms, ec);
  if (ec) {
    return std::unexpected(makeError(ErrorCode::kFilePermissionDenied, 
                                     "Cannot set directory permissions: " + ec.message()));
  }
  
  return {};
}

Result<void> FileSystem::moveFile(const std::filesystem::path& from, 
                                  const std::filesystem::path& to) {
  std::error_code ec;
  std::filesystem::rename(from, to, ec);
  
  if (ec) {
    return std::unexpected(makeError(ErrorCode::kFileWriteError, 
                                     "Move failed: " + ec.message()));
  }
  
  return {};
}

Result<void> FileSystem::copyFile(const std::filesystem::path& from, 
                                  const std::filesystem::path& to) {
  std::error_code ec;
  std::filesystem::copy_file(from, to, 
                             std::filesystem::copy_options::overwrite_existing, ec);
  
  if (ec) {
    return std::unexpected(makeError(ErrorCode::kFileWriteError, 
                                     "Copy failed: " + ec.message()));
  }
  
  return {};
}

Result<void> FileSystem::validatePath(const std::filesystem::path& path) {
  std::error_code ec;
  auto status = std::filesystem::status(path, ec);
  
  if (ec && ec != std::errc::no_such_file_or_directory) {
    return std::unexpected(makeError(ErrorCode::kFileReadError, 
                                     "Cannot access path: " + ec.message()));
  }
  
  // Check for symlink attacks (if path exists)
  if (!ec && std::filesystem::is_symlink(status)) {
    auto target = std::filesystem::read_symlink(path, ec);
    if (ec) {
      return std::unexpected(makeError(ErrorCode::kFileReadError, 
                                       "Cannot read symlink: " + ec.message()));
    }
    
    // Ensure symlink target is not outside allowed areas
    if (target.is_absolute() || target.string().find("..") != std::string::npos) {
      return std::unexpected(makeError(ErrorCode::kFilePermissionDenied, 
                                       "Symlink target not allowed: " + target.string()));
    }
  }
  
  return {};
}

Result<std::uintmax_t> FileSystem::fileSize(const std::filesystem::path& path) {
  std::error_code ec;
  auto size = std::filesystem::file_size(path, ec);
  
  if (ec) {
    return std::unexpected(makeError(ErrorCode::kFileReadError, 
                                     "Cannot get file size: " + ec.message()));
  }
  
  return size;
}

Result<std::filesystem::file_time_type> FileSystem::lastModified(const std::filesystem::path& path) {
  std::error_code ec;
  auto time = std::filesystem::last_write_time(path, ec);
  
  if (ec) {
    return std::unexpected(makeError(ErrorCode::kFileReadError, 
                                     "Cannot get modification time: " + ec.message()));
  }
  
  return time;
}

Result<void> FileSystem::removeFile(const std::filesystem::path& path) {
  std::error_code ec;
  std::filesystem::remove(path, ec);
  
  if (ec) {
    return std::unexpected(makeError(ErrorCode::kFileWriteError, 
                                     "Cannot remove file: " + ec.message()));
  }
  
  return {};
}

Result<std::vector<std::filesystem::path>> FileSystem::listDirectory(
    const std::filesystem::path& path, const std::string& extension_filter) {
  std::vector<std::filesystem::path> results;
  std::error_code ec;
  
  for (const auto& entry : std::filesystem::directory_iterator(path, ec)) {
    if (ec) {
      return std::unexpected(makeError(ErrorCode::kDirectoryNotFound, 
                                       "Cannot list directory: " + ec.message()));
    }
    
    if (entry.is_regular_file(ec) && !ec) {
      if (extension_filter.empty() || entry.path().extension() == extension_filter) {
        results.push_back(entry.path());
      }
    }
  }
  
  return results;
}

Result<void> FileSystem::ensureXdgDirectory(const std::filesystem::path& path) {
  return createDirectories(path, std::filesystem::perms::owner_all);
}

Result<std::uintmax_t> FileSystem::availableSpace(const std::filesystem::path& path) {
  std::error_code ec;
  auto space = std::filesystem::space(path, ec);
  
  if (ec) {
    return std::unexpected(makeError(ErrorCode::kFileReadError, 
                                     "Cannot get available space: " + ec.message()));
  }
  
  return space.available;
}

Result<void> FileSystem::syncDirectory(const std::filesystem::path& path) {
#ifdef _WIN32
  int fd = _open(path.string().c_str(), _O_RDONLY);
#else
  int fd = open(path.c_str(), O_RDONLY);
#endif
  if (fd < 0) {
    return std::unexpected(makeError(ErrorCode::kFileReadError, 
                                     "Cannot open directory for sync"));
  }
  
  auto result = fsyncFile(fd);
#ifdef _WIN32
  _close(fd);
#else
  close(fd);
#endif
  
  return result;
}

Result<void> FileSystem::fsyncFile(int fd) {
#ifdef _WIN32
  if (_commit(fd) < 0) {
    return std::unexpected(makeError(ErrorCode::kFileWriteError, "Sync failed"));
  }
#else
  if (fsync(fd) < 0) {
    return std::unexpected(makeError(ErrorCode::kFileWriteError, "Sync failed"));
  }
#endif
  return {};
}

Result<void> FileSystem::setFilePermissions(const std::filesystem::path& path, 
                                            std::filesystem::perms perms) {
  std::error_code ec;
  std::filesystem::permissions(path, perms, ec);
  
  if (ec) {
    return std::unexpected(makeError(ErrorCode::kFilePermissionDenied, 
                                     "Cannot set permissions: " + ec.message()));
  }
  
  return {};
}

}  // namespace nx::util