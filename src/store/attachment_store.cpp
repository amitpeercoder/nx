#include "nx/store/attachment_store.hpp"

#include <algorithm>

namespace nx::store {

std::string AttachmentInfo::storageFilename() const {
  // Sanitize original name
  std::string sanitized = original_name;
  
  // Replace invalid characters
  std::replace_if(sanitized.begin(), sanitized.end(), 
                  [](char c) { return c == '/' || c == '\\' || c == ':' || c == '*' || 
                               c == '?' || c == '"' || c == '<' || c == '>' || c == '|'; }, 
                  '_');
  
  // Limit length
  if (sanitized.length() > 100) {
    // Keep extension if present
    auto dot_pos = sanitized.find_last_of('.');
    if (dot_pos != std::string::npos && dot_pos > sanitized.length() - 10) {
      std::string extension = sanitized.substr(dot_pos);
      sanitized = sanitized.substr(0, 90 - extension.length()) + extension;
    } else {
      sanitized = sanitized.substr(0, 100);
    }
  }
  
  return id.toString() + "-" + sanitized;
}

std::string AttachmentInfo::relativePath() const {
  return "attachments/" + storageFilename();
}

}  // namespace nx::store