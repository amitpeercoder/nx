#pragma once

#include <string>
#include <optional>
#include "nx/common.hpp"

namespace nx::util {

/**
 * @brief Utilities for secure handling of sensitive data
 */
class Security {
public:
  /**
   * @brief Mask sensitive strings for logging/debug output
   * @param sensitive The sensitive string to mask
   * @param reveal_chars Number of characters to reveal at start/end (default: 4)
   * @return Masked string showing only first/last few characters
   */
  static std::string maskSensitive(const std::string& sensitive, size_t reveal_chars = 4);

  /**
   * @brief Validate API key format
   * @param api_key The API key to validate
   * @param provider The AI provider name (e.g., "openai", "anthropic")
   * @return true if key format is valid
   */
  static bool validateApiKeyFormat(const std::string& api_key, const std::string& provider);

  /**
   * @brief Securely zero out memory containing sensitive data
   * @param data Pointer to sensitive data
   * @param size Size of data in bytes
   */
  static void secureZero(void* data, size_t size);

  /**
   * @brief Clear sensitive string contents securely
   * @param sensitive String containing sensitive data
   */
  static void clearSensitiveString(std::string& sensitive);

  /**
   * @brief Generate a random string for temporary file names
   * @param length Length of random string
   * @return Random string
   */
  static std::string generateRandomString(size_t length = 16);

  /**
   * @brief Check if a string contains potentially sensitive data
   * @param text Text to check
   * @return true if text might contain sensitive data
   */
  static bool containsSensitiveData(const std::string& text);

private:
  Security() = default;
};

/**
 * @brief RAII wrapper for sensitive strings that auto-clears on destruction
 */
class SensitiveString {
public:
  explicit SensitiveString(std::string value);
  SensitiveString(const SensitiveString&) = delete;
  SensitiveString& operator=(const SensitiveString&) = delete;
  SensitiveString(SensitiveString&& other) noexcept;
  SensitiveString& operator=(SensitiveString&& other) noexcept;
  ~SensitiveString();

  const std::string& value() const { return value_; }
  std::string masked() const { return Security::maskSensitive(value_); }
  bool empty() const { return value_.empty(); }
  size_t size() const { return value_.size(); }
  
  void clear();

private:
  std::string value_;
};

} // namespace nx::util