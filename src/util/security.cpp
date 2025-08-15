#include "nx/util/security.hpp"

#include <algorithm>
#include <random>
#include <regex>
#include <cstring>

#ifdef __APPLE__
#include <Security/SecRandom.h>
#elif defined(__linux__)
#include <sys/random.h>
#endif

namespace nx::util {

std::string Security::maskSensitive(const std::string& sensitive, size_t reveal_chars) {
  if (sensitive.empty()) {
    return "[empty]";
  }
  
  if (sensitive.length() <= reveal_chars * 2) {
    // If string is too short, just show asterisks
    return std::string(std::min(sensitive.length(), size_t(8)), '*');
  }
  
  std::string masked;
  masked.reserve(sensitive.length());
  
  // Show first few characters
  masked.append(sensitive.substr(0, reveal_chars));
  
  // Add asterisks for middle part
  size_t middle_length = sensitive.length() - (reveal_chars * 2);
  masked.append(std::string(std::min(middle_length, size_t(12)), '*'));
  
  // Show last few characters
  masked.append(sensitive.substr(sensitive.length() - reveal_chars));
  
  return masked;
}

bool Security::validateApiKeyFormat(const std::string& api_key, const std::string& provider) {
  if (api_key.empty()) {
    return false;
  }
  
  // Basic length and character validation
  if (api_key.length() < 16 || api_key.length() > 256) {
    return false;
  }
  
  // Should contain only alphanumeric characters, dashes, and underscores
  std::regex valid_chars(R"([a-zA-Z0-9\-_]+)");
  if (!std::regex_match(api_key, valid_chars)) {
    return false;
  }
  
  // Provider-specific validation
  if (provider == "openai") {
    // OpenAI keys typically start with "sk-"
    return api_key.substr(0, 3) == "sk-" && api_key.length() >= 48;
  } else if (provider == "anthropic") {
    // Anthropic keys typically start with "sk-ant-"
    return api_key.substr(0, 7) == "sk-ant-" && api_key.length() >= 50;
  }
  
  // Generic validation for unknown providers
  return true;
}

void Security::secureZero(void* data, size_t size) {
  if (!data || size == 0) {
    return;
  }
  
#ifdef _WIN32
  SecureZeroMemory(data, size);
#else
  // Use volatile to prevent compiler optimization
  volatile unsigned char* ptr = static_cast<volatile unsigned char*>(data);
  for (size_t i = 0; i < size; ++i) {
    ptr[i] = 0;
  }
  
  // Memory barrier to prevent reordering
  __asm__ __volatile__("" : : "r"(ptr) : "memory");
#endif
}

void Security::clearSensitiveString(std::string& sensitive) {
  if (!sensitive.empty()) {
    // Zero out the memory
    secureZero(const_cast<char*>(sensitive.data()), sensitive.size());
    sensitive.clear();
    
    // Force deallocation to ensure memory is not accessible
    sensitive.shrink_to_fit();
  }
}

std::string Security::generateRandomString(size_t length) {
  const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
  const size_t charset_size = sizeof(charset) - 1;
  
  std::string result;
  result.reserve(length);
  
  // Use cryptographically secure random number generation if available
#ifdef __APPLE__
  std::vector<uint8_t> random_bytes(length);
  if (SecRandomCopyBytes(kSecRandomDefault, length, random_bytes.data()) == errSecSuccess) {
    for (size_t i = 0; i < length; ++i) {
      result += charset[random_bytes[i] % charset_size];
    }
    return result;
  }
#elif defined(__linux__)
  std::vector<uint8_t> random_bytes(length);
  if (getrandom(random_bytes.data(), length, 0) == static_cast<ssize_t>(length)) {
    for (size_t i = 0; i < length; ++i) {
      result += charset[random_bytes[i] % charset_size];
    }
    return result;
  }
#endif
  
  // Fallback to standard random number generator
  static thread_local std::random_device rd;
  static thread_local std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(0, charset_size - 1);
  
  for (size_t i = 0; i < length; ++i) {
    result += charset[dis(gen)];
  }
  
  return result;
}

bool Security::containsSensitiveData(const std::string& text) {
  // Check for patterns that might indicate sensitive data
  std::vector<std::regex> sensitive_patterns = {
    std::regex(R"(sk-[a-zA-Z0-9\-_]{40,})", std::regex_constants::icase),  // API keys
    std::regex(R"([a-zA-Z0-9]{32,})", std::regex_constants::icase),        // Long hex strings
    std::regex(R"(password|passwd|secret|token|key)", std::regex_constants::icase), // Sensitive keywords
    std::regex(R"([A-Za-z0-9+/]{40,}={0,2})", std::regex_constants::icase) // Base64 encoded data
  };
  
  for (const auto& pattern : sensitive_patterns) {
    if (std::regex_search(text, pattern)) {
      return true;
    }
  }
  
  return false;
}

// SensitiveString implementation

SensitiveString::SensitiveString(std::string value) : value_(std::move(value)) {}

SensitiveString::SensitiveString(SensitiveString&& other) noexcept : value_(std::move(other.value_)) {
  other.clear();
}

SensitiveString& SensitiveString::operator=(SensitiveString&& other) noexcept {
  if (this != &other) {
    clear();
    value_ = std::move(other.value_);
    other.clear();
  }
  return *this;
}

SensitiveString::~SensitiveString() {
  clear();
}

void SensitiveString::clear() {
  Security::clearSensitiveString(value_);
}

} // namespace nx::util