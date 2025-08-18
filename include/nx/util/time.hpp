#pragma once

#include <chrono>
#include <string>

#include "nx/common.hpp"

namespace nx::util {

// Time utilities for RFC3339 formatting and parsing
class Time {
 public:
  // Format time as RFC3339 string (ISO 8601)
  static std::string toRfc3339(std::chrono::system_clock::time_point time);

  // Parse RFC3339 string to time_point
  static Result<std::chrono::system_clock::time_point> fromRfc3339(const std::string& str);

  // Get current time
  static std::chrono::system_clock::time_point now();

  // Format duration for human reading
  static std::string formatDuration(std::chrono::nanoseconds duration);

  // Parse human-readable relative time (e.g., "2 days ago", "1 week")
  static Result<std::chrono::system_clock::time_point> parseRelativeTime(const std::string& str);
};

}  // namespace nx::util