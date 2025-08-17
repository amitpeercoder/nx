#pragma once

#include <chrono>
#include <string>
#include <string_view>

#include "nx/common.hpp"

namespace nx::core {

// ULID (Universally Unique Lexicographically Sortable Identifier)
// 26 characters, base32 encoded, sortable by time
class NoteId {
 public:
  // Create new ULID with current timestamp
  static NoteId generate();

  // Create ULID with specific timestamp
  static NoteId generate(std::chrono::system_clock::time_point timestamp);

  // Parse ULID from string
  static Result<NoteId> fromString(std::string_view str);

  // Default constructor creates invalid ID
  NoteId() = default;

  // Get string representation
  std::string toString() const;

  // Get timestamp component
  std::chrono::system_clock::time_point timestamp() const;

  // Comparison operators
  bool operator==(const NoteId& other) const noexcept;
  bool operator!=(const NoteId& other) const noexcept;
  bool operator<(const NoteId& other) const noexcept;
  bool operator<=(const NoteId& other) const noexcept;
  bool operator>(const NoteId& other) const noexcept;
  bool operator>=(const NoteId& other) const noexcept;

  // Check if ID is valid
  bool isValid() const noexcept;

  // Hash support for containers
  struct Hash {
    std::size_t operator()(const NoteId& id) const noexcept;
  };

 private:
  explicit NoteId(std::string id);

  // Validate ULID format
  static bool isValidFormat(std::string_view str);

  std::string id_;
};

}  // namespace nx::core

// Hash specialization for std::unordered_map
namespace std {
template <>
struct hash<nx::core::NoteId> : nx::core::NoteId::Hash {};
}  // namespace std