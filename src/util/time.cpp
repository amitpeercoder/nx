#include "nx/util/time.hpp"

#include <iomanip>
#include <regex>
#include <sstream>

namespace nx::util {

std::string Time::toRfc3339(std::chrono::system_clock::time_point time) {
  auto time_t = std::chrono::system_clock::to_time_t(time);
  auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
      time.time_since_epoch()) % 1000;
  
  std::ostringstream oss;
  oss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%S");
  oss << '.' << std::setfill('0') << std::setw(3) << milliseconds.count() << 'Z';
  
  return oss.str();
}

Result<std::chrono::system_clock::time_point> Time::fromRfc3339(const std::string& str) {
  std::regex rfc3339_regex(
      R"((\d{4})-(\d{2})-(\d{2})T(\d{2}):(\d{2}):(\d{2})(?:\.(\d{3}))?Z?)");
  
  std::smatch match;
  if (!std::regex_match(str, match, rfc3339_regex)) {
    return std::unexpected(makeError(ErrorCode::kParseError, 
                                     "Invalid RFC3339 format: " + str));
  }
  
  std::tm tm = {};
  tm.tm_year = std::stoi(match[1]) - 1900;
  tm.tm_mon = std::stoi(match[2]) - 1;
  tm.tm_mday = std::stoi(match[3]);
  tm.tm_hour = std::stoi(match[4]);
  tm.tm_min = std::stoi(match[5]);
  tm.tm_sec = std::stoi(match[6]);
  
  auto time_t = std::mktime(&tm);
  if (time_t == -1) {
    return std::unexpected(makeError(ErrorCode::kParseError, 
                                     "Invalid time values: " + str));
  }
  
  auto time_point = std::chrono::system_clock::from_time_t(time_t);
  
  // Add milliseconds if present
  if (match[7].matched) {
    int milliseconds = std::stoi(match[7]);
    time_point += std::chrono::milliseconds(milliseconds);
  }
  
  return time_point;
}

std::chrono::system_clock::time_point Time::now() {
  return std::chrono::system_clock::now();
}

std::string Time::formatDuration(std::chrono::nanoseconds duration) {
  auto hours = std::chrono::duration_cast<std::chrono::hours>(duration);
  duration -= hours;
  auto minutes = std::chrono::duration_cast<std::chrono::minutes>(duration);
  duration -= minutes;
  auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration);
  duration -= seconds;
  auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
  
  std::ostringstream oss;
  
  if (hours.count() > 0) {
    oss << hours.count() << "h ";
  }
  if (minutes.count() > 0) {
    oss << minutes.count() << "m ";
  }
  if (seconds.count() > 0 || (hours.count() == 0 && minutes.count() == 0)) {
    oss << seconds.count();
    if (milliseconds.count() > 0 && hours.count() == 0 && minutes.count() == 0) {
      oss << "." << std::setfill('0') << std::setw(3) << milliseconds.count();
    }
    oss << "s";
  }
  
  std::string result = oss.str();
  if (!result.empty() && result.back() == ' ') {
    result.pop_back();
  }
  
  return result.empty() ? "0s" : result;
}

Result<std::chrono::system_clock::time_point> Time::parseRelativeTime(const std::string& str) {
  std::regex relative_regex(R"((\d+)\s*(second|minute|hour|day|week|month|year)s?\s*ago)");
  
  std::smatch match;
  if (!std::regex_match(str, match, relative_regex)) {
    return std::unexpected(makeError(ErrorCode::kParseError, 
                                     "Invalid relative time format: " + str));
  }
  
  int amount = std::stoi(match[1]);
  std::string unit = match[2];
  
  auto now = std::chrono::system_clock::now();
  
  if (unit == "second") {
    return now - std::chrono::seconds(amount);
  } else if (unit == "minute") {
    return now - std::chrono::minutes(amount);
  } else if (unit == "hour") {
    return now - std::chrono::hours(amount);
  } else if (unit == "day") {
    return now - std::chrono::hours(24 * amount);
  } else if (unit == "week") {
    return now - std::chrono::hours(24 * 7 * amount);
  } else if (unit == "month") {
    return now - std::chrono::hours(24 * 30 * amount);  // Approximate
  } else if (unit == "year") {
    return now - std::chrono::hours(24 * 365 * amount);  // Approximate
  }
  
  return std::unexpected(makeError(ErrorCode::kParseError, 
                                   "Unknown time unit: " + unit));
}

}  // namespace nx::util