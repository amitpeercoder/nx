#include "nx/tui/editor_security.hpp"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <regex>
#include <sstream>
#include <unicode/utf8.h>
#include <unicode/utypes.h>
#include "nx/util/safe_process.hpp"

namespace nx::tui {

// EditorInputValidator Implementation

EditorInputValidator::EditorInputValidator(ValidationConfig config) 
    : config_(std::move(config)) {
}

Result<char> EditorInputValidator::validateCharacter(char ch, size_t current_line_length) const {
    // Check line length bounds
    if (current_line_length >= config_.max_line_length) {
        return makeErrorResult<char>(ErrorCode::kValidationError, 
            "Line length exceeds maximum allowed (" + std::to_string(config_.max_line_length) + ")");
    }

    // Check for dangerous terminal escape sequences
    if (isDangerousEscape(ch)) {
        return makeErrorResult<char>(ErrorCode::kSecurityError, 
            "Character could initiate dangerous terminal escape sequence");
    }

    // Check control characters
    if (!config_.allow_control_chars && isControlCharacter(ch)) {
        // Allow common control chars: newline, tab, carriage return
        if (ch != '\n' && ch != '\t' && ch != '\r') {
            return makeErrorResult<char>(ErrorCode::kValidationError, 
                "Control character not allowed in editor input");
        }
    }

    // Character is valid
    return ch;
}

Result<std::string> EditorInputValidator::validateString(const std::string& input, size_t current_total_size) const {
    // Check total size bounds
    if (current_total_size + input.size() > config_.max_total_size) {
        return makeErrorResult<std::string>(ErrorCode::kValidationError, 
            "Adding string would exceed maximum file size (" + std::to_string(config_.max_total_size) + ")");
    }

    // Validate UTF-8 if strict mode enabled
    if (config_.strict_utf8) {
        std::vector<uint8_t> bytes(input.begin(), input.end());
        auto utf8_result = validateUtf8Sequence(bytes);
        if (!utf8_result) {
            return std::unexpected(utf8_result.error());
        }
    }

    // Note: We sanitize dangerous terminal escapes instead of rejecting
    // The sanitizeInput() call below will handle this

    // Count newlines for line limit validation
    size_t newline_count = std::count(input.begin(), input.end(), '\n');
    auto line_result = validateLineCount(0, newline_count); // Current lines checked elsewhere
    if (!line_result) {
        return std::unexpected(line_result.error());
    }

    return sanitizeInput(input);
}

Result<std::string> EditorInputValidator::validateUtf8Sequence(const std::vector<uint8_t>& utf8_bytes) const {
    if (utf8_bytes.empty()) {
        return std::string();
    }

    for (size_t i = 0; i < utf8_bytes.size();) {
        uint8_t first_byte = utf8_bytes[i];
        
        // ASCII character (0xxxxxxx)
        if ((first_byte & 0x80) == 0) {
            i++;
            continue;
        }

        // Multi-byte UTF-8 sequence
        size_t sequence_length = getUtf8SequenceLength(first_byte);
        if (sequence_length == 0) {
            return makeErrorResult<std::string>(ErrorCode::kValidationError, 
                "Invalid UTF-8 start byte: " + std::to_string(first_byte));
        }

        // Check if we have enough bytes
        if (i + sequence_length > utf8_bytes.size()) {
            return makeErrorResult<std::string>(ErrorCode::kValidationError, 
                "Incomplete UTF-8 sequence at end of input");
        }

        // Validate continuation bytes
        for (size_t j = 1; j < sequence_length; j++) {
            if (!isValidUtf8Continuation(utf8_bytes[i + j])) {
                return makeErrorResult<std::string>(ErrorCode::kValidationError, 
                    "Invalid UTF-8 continuation byte");
            }
        }

        // Check for overlong encodings and other invalid patterns
        if (sequence_length == 2) {
            // 2-byte sequences: 110xxxxx 10xxxxxx
            // Must encode values >= 0x80 (128)
            // 0xC0 0x80 encodes 0x00 (overlong)
            // 0xC1 0xBF encodes 0x7F (overlong)
            if (first_byte <= 0xC1) {
                return makeErrorResult<std::string>(ErrorCode::kValidationError, 
                    "Overlong UTF-8 encoding detected");
            }
        } else if (sequence_length == 3) {
            // 3-byte sequences: 1110xxxx 10xxxxxx 10xxxxxx
            // Must encode values >= 0x800 (2048)
            // 0xE0 0x80 0x80 encodes 0x00 (overlong)
            if (first_byte == 0xE0 && utf8_bytes[i + 1] < 0xA0) {
                return makeErrorResult<std::string>(ErrorCode::kValidationError, 
                    "Overlong UTF-8 encoding detected");
            }
        } else if (sequence_length == 4) {
            // 4-byte sequences: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx  
            // Must encode values >= 0x10000 (65536)
            // 0xF0 0x80 0x80 0x80 encodes 0x00 (overlong)
            if (first_byte == 0xF0 && utf8_bytes[i + 1] < 0x90) {
                return makeErrorResult<std::string>(ErrorCode::kValidationError, 
                    "Overlong UTF-8 encoding detected");
            }
            // Also check for values above valid Unicode range (> 0x10FFFF)
            if (first_byte > 0xF4 || 
                (first_byte == 0xF4 && utf8_bytes[i + 1] > 0x8F)) {
                return makeErrorResult<std::string>(ErrorCode::kValidationError, 
                    "UTF-8 sequence encodes invalid Unicode codepoint");
            }
        }

        i += sequence_length;
    }

    // If we get here, UTF-8 is valid
    return std::string(utf8_bytes.begin(), utf8_bytes.end());
}

bool EditorInputValidator::isDangerousEscape(char ch) const {
    // Characters that could start dangerous ANSI/terminal escape sequences
    static const char dangerous_chars[] = {
        '\x1b',  // ESC - starts ANSI escape sequences
        '\x07',  // BEL - terminal bell, could be abused
        '\x0e',  // SO - shift out
        '\x0f',  // SI - shift in
        '\x7f',  // DEL - delete character
    };

    return std::find(std::begin(dangerous_chars), std::end(dangerous_chars), ch) != std::end(dangerous_chars);
}

std::string EditorInputValidator::sanitizeInput(const std::string& input) const {
    std::string sanitized;
    sanitized.reserve(input.size());

    for (char ch : input) {
        if (isDangerousEscape(ch)) {
            // Replace dangerous characters with safe placeholder
            sanitized += '?';
        } else if (!config_.allow_control_chars && isControlCharacter(ch)) {
            // Replace control chars (except allowed ones) with space
            if (ch == '\n' || ch == '\t' || ch == '\r') {
                sanitized += ch;
            } else {
                sanitized += ' ';
            }
        } else {
            sanitized += ch;
        }
    }

    return sanitized;
}

Result<void> EditorInputValidator::validateLineCount(size_t current_lines, size_t additional_lines) const {
    if (current_lines + additional_lines > config_.max_lines) {
        return makeErrorResult<void>(ErrorCode::kValidationError, 
            "Line count would exceed maximum allowed (" + std::to_string(config_.max_lines) + ")");
    }
    return {};
}

bool EditorInputValidator::isControlCharacter(char ch) const {
    return (ch >= 0 && ch <= 31) || ch == 127;
}

bool EditorInputValidator::isValidUtf8Start(uint8_t byte) const {
    // Valid UTF-8 start bytes:
    // 0xxxxxxx (ASCII)
    // 110xxxxx (2-byte sequence start)
    // 1110xxxx (3-byte sequence start)  
    // 11110xxx (4-byte sequence start)
    return (byte & 0x80) == 0 ||           // ASCII
           (byte & 0xe0) == 0xc0 ||        // 2-byte start
           (byte & 0xf0) == 0xe0 ||        // 3-byte start
           (byte & 0xf8) == 0xf0;          // 4-byte start
}

bool EditorInputValidator::isValidUtf8Continuation(uint8_t byte) const {
    // UTF-8 continuation bytes: 10xxxxxx
    return (byte & 0xc0) == 0x80;
}

size_t EditorInputValidator::getUtf8SequenceLength(uint8_t first_byte) const {
    if ((first_byte & 0x80) == 0) return 1;     // ASCII
    if ((first_byte & 0xe0) == 0xc0) return 2;  // 2-byte sequence
    if ((first_byte & 0xf0) == 0xe0) return 3;  // 3-byte sequence
    if ((first_byte & 0xf8) == 0xf0) return 4;  // 4-byte sequence
    return 0; // Invalid
}

// SecureClipboard Implementation

SecureClipboard::SecureClipboard() 
    : content_(nullptr), system_clipboard_available_(false) {
    detectSystemClipboard();
}

SecureClipboard::~SecureClipboard() {
    clear();
}

Result<void> SecureClipboard::setContent(const std::string& content) {
    // Clear existing content securely
    clear();

    try {
        content_ = std::make_unique<nx::util::SensitiveString>(content);
        
        // Try to set system clipboard if available
        if (system_clipboard_available_) {
            setSystemClipboard(content);
        }
        
        return {};
    } catch (const std::exception& e) {
        return makeErrorResult<void>(ErrorCode::kSystemError, 
            "Failed to set clipboard content: " + std::string(e.what()));
    }
}

Result<std::string> SecureClipboard::getContent() const {
    if (!content_) {
        // Try to get from system clipboard if available
        if (system_clipboard_available_) {
            auto system_content = getSystemClipboard();
            if (system_content) {
                return *system_content;
            }
        }
        
        return makeErrorResult<std::string>(ErrorCode::kNotFound, "Clipboard is empty");
    }

    return content_->value();
}

void SecureClipboard::clear() {
    if (content_) {
        content_->clear();
        content_.reset();
    }
}

bool SecureClipboard::hasContent() const {
    return content_ != nullptr && !content_->empty();
}

bool SecureClipboard::trySystemClipboard() {
    detectSystemClipboard();
    return system_clipboard_available_;
}

size_t SecureClipboard::getContentSize() const {
    return content_ ? content_->size() : 0;
}

bool SecureClipboard::setSystemClipboard(const std::string& content) {
    // Try different system clipboard tools using secure process execution
    
    // macOS: pbcopy
    if (nx::util::SafeProcess::commandExists("pbcopy")) {
        // Use echo with proper escaping to pipe content to pbcopy
        auto echo_result = nx::util::SafeProcess::execute("sh", {"-c", "echo " + nx::util::SafeProcess::escapeArgument(content) + " | pbcopy"});
        if (echo_result.has_value() && echo_result->success()) {
            return true;
        }
    }
    
    // Linux X11: xclip
    if (nx::util::SafeProcess::commandExists("xclip")) {
        auto echo_result = nx::util::SafeProcess::execute("sh", {"-c", "echo " + nx::util::SafeProcess::escapeArgument(content) + " | xclip -selection clipboard"});
        if (echo_result.has_value() && echo_result->success()) {
            return true;
        }
    }
    
    // Linux Wayland: wl-clipboard
    if (nx::util::SafeProcess::commandExists("wl-copy")) {
        auto echo_result = nx::util::SafeProcess::execute("sh", {"-c", "echo " + nx::util::SafeProcess::escapeArgument(content) + " | wl-copy"});
        if (echo_result.has_value() && echo_result->success()) {
            return true;
        }
    }
    
    return false;
}

std::optional<std::string> SecureClipboard::getSystemClipboard() const {
    // Try different system clipboard tools using secure process execution
    
    // macOS: pbpaste
    if (nx::util::SafeProcess::commandExists("pbpaste")) {
        auto result = nx::util::SafeProcess::executeForOutput("pbpaste", {});
        if (result.has_value()) {
            return result.value();
        }
    }
    
    // Linux X11: xclip
    if (nx::util::SafeProcess::commandExists("xclip")) {
        auto result = nx::util::SafeProcess::executeForOutput("xclip", {"-selection", "clipboard", "-o"});
        if (result.has_value()) {
            return result.value();
        }
    }
    
    // Linux Wayland: wl-clipboard
    if (nx::util::SafeProcess::commandExists("wl-paste")) {
        auto result = nx::util::SafeProcess::executeForOutput("wl-paste", {});
        if (result.has_value()) {
            return result.value();
        }
    }
    
    return std::nullopt;
}

void SecureClipboard::detectSystemClipboard() {
    // Check for available system clipboard tools using secure process execution
    system_clipboard_available_ = 
        nx::util::SafeProcess::commandExists("pbcopy") ||     // macOS
        nx::util::SafeProcess::commandExists("xclip") ||      // Linux X11
        nx::util::SafeProcess::commandExists("wl-copy");      // Linux Wayland
}

// EditorBoundsChecker Implementation

Result<void> EditorBoundsChecker::validateLineIndex(size_t line_index, size_t total_lines) {
    if (total_lines == 0) {
        return makeErrorResult<void>(ErrorCode::kValidationError, "Line index out of bounds: empty document");
    }
    
    if (line_index >= total_lines) {
        return makeErrorResult<void>(ErrorCode::kValidationError, 
            "Line index " + std::to_string(line_index) + " >= total lines " + std::to_string(total_lines));
    }
    
    return {};
}

Result<void> EditorBoundsChecker::validateColumnIndex(size_t col_index, size_t line_length) {
    if (col_index > line_length) {
        return makeErrorResult<void>(ErrorCode::kValidationError, 
            "Column index " + std::to_string(col_index) + " > line length " + std::to_string(line_length));
    }
    
    return {};
}

Result<std::pair<size_t, size_t>> EditorBoundsChecker::validateCursorPosition(
    size_t line, size_t col, const std::vector<std::string>& lines) {
    
    if (lines.empty()) {
        return std::make_pair(0, 0);
    }
    
    // Clamp line to valid range
    size_t clamped_line = std::min(line, lines.size() - 1);
    
    // Clamp column to valid range for the line
    size_t line_length = safeStringLength(lines[clamped_line]);
    size_t clamped_col = std::min(col, line_length);
    
    return std::make_pair(clamped_line, clamped_col);
}

Result<void> EditorBoundsChecker::validateMemoryUsage(size_t current_size, size_t additional_size, size_t max_size) {
    if (current_size > max_size) {
        return makeErrorResult<void>(ErrorCode::kValidationError, "Current memory usage already exceeds limit");
    }
    
    if (current_size + additional_size > max_size) {
        return makeErrorResult<void>(ErrorCode::kValidationError, 
            "Memory allocation would exceed limit: " + std::to_string(current_size + additional_size) + 
            " > " + std::to_string(max_size));
    }
    
    return {};
}

size_t EditorBoundsChecker::safeStringLength(const std::string& str) {
    if (str.empty()) {
        return 0;
    }
    
    // Use ICU for proper UTF-8 character counting
    UErrorCode status = U_ZERO_ERROR;
    int32_t utf8_length = static_cast<int32_t>(str.length());
    int32_t char_count = 0;
    
    // Count Unicode code points (characters)
    int32_t offset = 0;
    while (offset < utf8_length) {
        UChar32 codepoint;
        U8_NEXT(str.c_str(), offset, utf8_length, codepoint);
        if (codepoint >= 0) {  // Valid code point
            char_count++;
        }
    }
    
    return static_cast<size_t>(char_count);
}

Result<std::string> EditorBoundsChecker::safeSubstring(const std::string& str, size_t start, size_t length) {
    if (start > str.length()) {
        return makeErrorResult<std::string>(ErrorCode::kValidationError, "Substring start position out of bounds");
    }
    
    size_t safe_length = std::min(length, str.length() - start);
    
    try {
        return str.substr(start, safe_length);
    } catch (const std::exception& e) {
        return makeErrorResult<std::string>(ErrorCode::kSystemError, "Failed to extract substring: " + std::string(e.what()));
    }
}

} // namespace nx::tui