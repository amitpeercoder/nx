#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <optional>
#include <set>

#include "nx/common.hpp"
#include "nx/core/note_id.hpp"

namespace nx::crypto {

/**
 * @brief Age/rage encryption interface for secure note storage
 * 
 * This class provides secure encryption/decryption of notes using the age/rage
 * encryption tool via secure subprocess calls. It ensures that plaintext data
 * never persists to disk when encryption is enabled.
 */
class AgeCrypto {
public:
  /**
   * @brief Encryption key information
   */
  struct KeyInfo {
    std::string public_key;   // Age public key (age1...)
    std::string fingerprint;  // Key fingerprint for identification
    std::string comment;      // Optional comment/description
  };

  /**
   * @brief Initialize encryption with a key file
   * @param key_file_path Path to age private key file
   * @return Success or error
   */
  static Result<AgeCrypto> initialize(const std::filesystem::path& key_file_path);

  /**
   * @brief Generate a new age key pair
   * @param key_file_path Where to store the private key
   * @param passphrase Optional passphrase for key protection
   * @return Key information or error
   */
  static Result<KeyInfo> generateKeyPair(
    const std::filesystem::path& key_file_path,
    const std::optional<std::string>& passphrase = std::nullopt
  );

  /**
   * @brief Get public key information
   * @return Key information or error
   */
  Result<KeyInfo> getKeyInfo() const;

  /**
   * @brief Encrypt note content
   * @param content Plaintext content to encrypt
   * @param note_id Note ID (used for additional authenticated data)
   * @return Encrypted content or error
   */
  Result<std::string> encrypt(const std::string& content, const nx::core::NoteId& note_id) const;

  /**
   * @brief Decrypt note content
   * @param encrypted_content Encrypted content
   * @param note_id Note ID (used for verification)
   * @return Plaintext content or error
   */
  Result<std::string> decrypt(const std::string& encrypted_content, const nx::core::NoteId& note_id) const;

  /**
   * @brief Encrypt a file in place
   * @param file_path Path to file to encrypt
   * @param note_id Note ID for additional data
   * @return Success or error
   */
  Result<void> encryptFile(const std::filesystem::path& file_path, const nx::core::NoteId& note_id) const;

  /**
   * @brief Decrypt a file in place
   * @param file_path Path to encrypted file
   * @param note_id Note ID for verification
   * @return Success or error
   */
  Result<void> decryptFile(const std::filesystem::path& file_path, const nx::core::NoteId& note_id) const;

  /**
   * @brief Check if a file is encrypted
   * @param file_path Path to file to check
   * @return true if file appears to be age-encrypted
   */
  static bool isFileEncrypted(const std::filesystem::path& file_path);

  /**
   * @brief Check if age/rage tools are available
   * @return true if encryption tools are available
   */
  static bool isAvailable();

  /**
   * @brief Get the recommended file extension for encrypted files
   * @return File extension (e.g., ".age")
   */
  static std::string getEncryptedExtension() { return ".age"; }

private:
  explicit AgeCrypto(std::filesystem::path key_file_path);

  std::filesystem::path key_file_path_;

  /**
   * @brief Create secure temporary file with O_TMPFILE if available
   * @param content Content to write to temp file
   * @return Path to temporary file or error
   */
  Result<std::filesystem::path> createSecureTempFile(const std::string& content) const;

  /**
   * @brief Read and securely delete a temporary file
   * @param temp_path Path to temporary file
   * @return File contents or error
   */
  Result<std::string> readAndDeleteSecureTempFile(const std::filesystem::path& temp_path) const;

  /**
   * @brief Verify age tool availability and version
   * @return Success or error with details
   */
  static Result<void> verifyAgeTools();
};

/**
 * @brief Encryption manager for handling encrypted notes
 * 
 * Provides high-level interface for managing encrypted notes within the
 * note store, handling key management and secure operations.
 */
class EncryptionManager {
public:
  /**
   * @brief Initialize encryption manager
   * @param crypto_config Encryption configuration
   * @return Encryption manager or error
   */
  static Result<EncryptionManager> initialize(const std::filesystem::path& key_file);

  /**
   * @brief Check if note should be encrypted
   * @param note_id Note ID to check
   * @return true if note should be encrypted
   */
  bool shouldEncrypt(const nx::core::NoteId& note_id) const;

  /**
   * @brief Encrypt a note if needed
   * @param content Note content
   * @param note_id Note ID
   * @return Potentially encrypted content or error
   */
  Result<std::string> encryptIfNeeded(const std::string& content, const nx::core::NoteId& note_id) const;

  /**
   * @brief Decrypt a note if needed
   * @param content Potentially encrypted content
   * @param note_id Note ID
   * @return Plaintext content or error
   */
  Result<std::string> decryptIfNeeded(const std::string& content, const nx::core::NoteId& note_id) const;

  /**
   * @brief Toggle encryption for a specific note
   * @param note_id Note ID
   * @param encrypt true to encrypt, false to decrypt
   * @return Success or error
   */
  Result<void> toggleNoteEncryption(const nx::core::NoteId& note_id, bool encrypt);

  /**
   * @brief Get encryption status for a note
   * @param note_id Note ID
   * @return true if note is encrypted
   */
  bool isNoteEncrypted(const nx::core::NoteId& note_id) const;

private:
  explicit EncryptionManager(AgeCrypto crypto);

  AgeCrypto crypto_;
  std::set<std::string> encrypted_notes_; // Set of encrypted note IDs
};

} // namespace nx::crypto