#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <optional>
#include <memory>
#include "nx/common.hpp"
#include "nx/util/security.hpp"

namespace nx::tui {

/**
 * @brief Security-focused input validator for TUI editor
 * 
 * Validates all user input to prevent terminal injection attacks,
 * buffer overflows, and other security vulnerabilities.
 */
class EditorInputValidator {
public:
    /**
     * @brief Configuration for input validation
     */
    struct ValidationConfig {
        size_t max_line_length;        // Maximum characters per line
        size_t max_total_size; // 100MB max file size
        size_t max_lines;             // Maximum number of lines
        bool allow_control_chars;       // Allow control characters
        bool strict_utf8;                // Enforce valid UTF-8
        bool allow_terminal_escapes;    // Block terminal escape sequences
        
        ValidationConfig() 
            : max_line_length(10000)
            , max_total_size(100 * 1024 * 1024)
            , max_lines(1000000)
            , allow_control_chars(false)
            , strict_utf8(true)
            , allow_terminal_escapes(false) {}
    };

    explicit EditorInputValidator(ValidationConfig config = ValidationConfig{});

    /**
     * @brief Validate a single character input
     * @param ch Character to validate
     * @param current_line_length Current line length for bounds checking
     * @return Result with validated character or error
     */
    Result<char> validateCharacter(char ch, size_t current_line_length) const;

    /**
     * @brief Validate a string input (paste operations, etc.)
     * @param input String to validate
     * @param current_total_size Current total document size
     * @return Result with sanitized string or error
     */
    Result<std::string> validateString(const std::string& input, size_t current_total_size) const;

    /**
     * @brief Validate UTF-8 sequence
     * @param utf8_bytes UTF-8 byte sequence
     * @return Result with validated sequence or error
     */
    Result<std::string> validateUtf8Sequence(const std::vector<uint8_t>& utf8_bytes) const;

    /**
     * @brief Check if character is a dangerous terminal escape
     * @param ch Character to check
     * @return true if character could start a dangerous escape sequence
     */
    bool isDangerousEscape(char ch) const;

    /**
     * @brief Sanitize string by removing/replacing dangerous characters
     * @param input Input string to sanitize
     * @return Sanitized string
     */
    std::string sanitizeInput(const std::string& input) const;

    /**
     * @brief Validate line count doesn't exceed limits
     * @param current_lines Current number of lines
     * @param additional_lines Lines being added
     * @return Result indicating success or bounds error
     */
    Result<void> validateLineCount(size_t current_lines, size_t additional_lines) const;

    /**
     * @brief Get current validation configuration
     */
    const ValidationConfig& getConfig() const { return config_; }

    /**
     * @brief Update validation configuration
     */
    void updateConfig(const ValidationConfig& new_config) { config_ = new_config; }

private:
    ValidationConfig config_;

    bool isControlCharacter(char ch) const;
    bool isValidUtf8Start(uint8_t byte) const;
    bool isValidUtf8Continuation(uint8_t byte) const;
    size_t getUtf8SequenceLength(uint8_t first_byte) const;
};

/**
 * @brief Secure clipboard handler for editor operations
 * 
 * Uses SensitiveString for secure memory handling and prevents
 * clipboard data leakage in encrypted environments.
 */
class SecureClipboard {
public:
    SecureClipboard();
    ~SecureClipboard();

    /**
     * @brief Set clipboard content securely
     * @param content Content to store in clipboard
     * @return Result indicating success or error
     */
    Result<void> setContent(const std::string& content);

    /**
     * @brief Get clipboard content
     * @return Result with clipboard content or error
     */
    Result<std::string> getContent() const;

    /**
     * @brief Clear clipboard content securely
     */
    void clear();

    /**
     * @brief Check if clipboard has content
     */
    bool hasContent() const;

    /**
     * @brief Try to integrate with system clipboard
     * @return true if system clipboard is available
     */
    bool trySystemClipboard();

    /**
     * @brief Get clipboard content size without exposing content
     */
    size_t getContentSize() const;

    /**
     * @brief Disable system clipboard integration (for testing)
     */
    void disableSystemClipboard() { system_clipboard_available_ = false; }

private:
    std::unique_ptr<nx::util::SensitiveString> content_;
    bool system_clipboard_available_;
    
    bool setSystemClipboard(const std::string& content);
    std::optional<std::string> getSystemClipboard() const;
    void detectSystemClipboard();
};

/**
 * @brief Memory-safe bounds checker for editor operations
 * 
 * Prevents buffer overflows and out-of-bounds access during
 * all editor operations.
 */
class EditorBoundsChecker {
public:
    /**
     * @brief Check if line index is valid
     * @param line_index Line index to check
     * @param total_lines Total number of lines
     * @return Result indicating validity or bounds error
     */
    static Result<void> validateLineIndex(size_t line_index, size_t total_lines);

    /**
     * @brief Check if column index is valid for given line
     * @param col_index Column index to check
     * @param line_length Length of the line
     * @return Result indicating validity or bounds error
     */
    static Result<void> validateColumnIndex(size_t col_index, size_t line_length);

    /**
     * @brief Validate cursor position
     * @param line Line index
     * @param col Column index
     * @param lines Vector of lines to validate against
     * @return Result with clamped position or error
     */
    static Result<std::pair<size_t, size_t>> validateCursorPosition(
        size_t line, size_t col, const std::vector<std::string>& lines);

    /**
     * @brief Check memory usage limits
     * @param current_size Current memory usage
     * @param additional_size Additional memory being allocated
     * @param max_size Maximum allowed memory
     * @return Result indicating if allocation is safe
     */
    static Result<void> validateMemoryUsage(size_t current_size, size_t additional_size, size_t max_size);

    /**
     * @brief Safe string length calculation with UTF-8 awareness
     * @param str String to measure
     * @return Character count (not byte count)
     */
    static size_t safeStringLength(const std::string& str);

    /**
     * @brief Safe substring extraction with bounds checking
     * @param str Source string
     * @param start Start position
     * @param length Length to extract
     * @return Result with substring or bounds error
     */
    static Result<std::string> safeSubstring(const std::string& str, size_t start, size_t length);
};

} // namespace nx::tui