#include "nx/core/note_id.hpp"

#include <array>
#include <random>
#include <regex>

namespace nx::core {

namespace {

// Base32 encoding for ULID (Crockford's Base32)
constexpr char kBase32[] = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";
constexpr size_t kBase32Size = 32;
constexpr size_t kUlidLength = 26;
constexpr size_t kTimestampLength = 10;
constexpr size_t kRandomnessLength = 16;

// Convert milliseconds to base32 timestamp
std::string encodeTimestamp(uint64_t milliseconds) {
  std::string result(kTimestampLength, '0');
  
  for (int i = kTimestampLength - 1; i >= 0; --i) {
    result[i] = kBase32[milliseconds % kBase32Size];
    milliseconds /= kBase32Size;
  }
  
  return result;
}

// Generate random suffix
std::string generateRandomness() {
  static thread_local std::random_device rd;
  static thread_local std::mt19937_64 gen(rd());
  static thread_local std::uniform_int_distribution<> dis(0, kBase32Size - 1);
  
  std::string result;
  result.reserve(kRandomnessLength);
  
  for (size_t i = 0; i < kRandomnessLength; ++i) {
    result += kBase32[dis(gen)];
  }
  
  return result;
}

// Decode base32 character
int decodeBase32Char(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'Z') {
    if (c == 'I' || c == 'L' || c == 'O' || c == 'U') return -1;  // Invalid chars
    if (c < 'I') return c - 'A' + 10;
    if (c < 'L') return c - 'A' + 9;
    if (c < 'O') return c - 'A' + 8;
    if (c < 'U') return c - 'A' + 7;
    return c - 'A' + 6;
  }
  return -1;
}

// Decode timestamp from base32
uint64_t decodeTimestamp(std::string_view timestamp) {
  uint64_t result = 0;
  
  for (char c : timestamp) {
    int value = decodeBase32Char(c);
    if (value < 0) return 0;  // Invalid character
    result = result * kBase32Size + value;
  }
  
  return result;
}

}  // namespace

NoteId NoteId::generate() {
  return generate(std::chrono::system_clock::now());
}

NoteId NoteId::generate(std::chrono::system_clock::time_point timestamp) {
  auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
      timestamp.time_since_epoch()).count();
  
  std::string ulid = encodeTimestamp(static_cast<uint64_t>(milliseconds));
  ulid += generateRandomness();
  
  return NoteId(std::move(ulid));
}

Result<NoteId> NoteId::fromString(std::string_view str) {
  if (!isValidFormat(str)) {
    return std::unexpected(makeError(ErrorCode::kInvalidArgument, 
                                     "Invalid ULID format: " + std::string(str)));
  }
  
  return NoteId(std::string(str));
}

std::string NoteId::toString() const {
  return id_;
}

std::chrono::system_clock::time_point NoteId::timestamp() const {
  if (!isValid()) {
    return std::chrono::system_clock::time_point{};
  }
  
  uint64_t milliseconds = decodeTimestamp(id_.substr(0, kTimestampLength));
  return std::chrono::system_clock::time_point{
      std::chrono::milliseconds(milliseconds)
  };
}

bool NoteId::operator==(const NoteId& other) const noexcept {
  return id_ == other.id_;
}

bool NoteId::operator!=(const NoteId& other) const noexcept {
  return !(*this == other);
}

bool NoteId::operator<(const NoteId& other) const noexcept {
  return id_ < other.id_;
}

bool NoteId::operator<=(const NoteId& other) const noexcept {
  return id_ <= other.id_;
}

bool NoteId::operator>(const NoteId& other) const noexcept {
  return id_ > other.id_;
}

bool NoteId::operator>=(const NoteId& other) const noexcept {
  return id_ >= other.id_;
}

bool NoteId::isValid() const noexcept {
  return !id_.empty() && isValidFormat(id_);
}

std::size_t NoteId::Hash::operator()(const NoteId& id) const noexcept {
  return std::hash<std::string>{}(id.id_);
}

NoteId::NoteId(std::string id) : id_(std::move(id)) {}

bool NoteId::isValidFormat(std::string_view str) {
  if (str.length() != kUlidLength) {
    return false;
  }
  
  // Check each character is valid base32
  for (char c : str) {
    if (decodeBase32Char(c) < 0) {
      return false;
    }
  }
  
  return true;
}

}  // namespace nx::core