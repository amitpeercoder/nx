#include <gtest/gtest.h>
#include "nx/tui/editor_security.hpp"
#include <string>
#include <vector>

using namespace nx::tui;
using namespace nx;

class EditorSecurityTest : public ::testing::Test {
protected:
    void SetUp() override {
        validator_ = std::make_unique<EditorInputValidator>();
        clipboard_ = std::make_unique<SecureClipboard>();
        clipboard_->disableSystemClipboard(); // Disable system clipboard for tests
        clipboard_->clear(); // Ensure clean state for tests
    }

    void TearDown() override {
        clipboard_.reset();
        validator_.reset();
    }

    std::unique_ptr<EditorInputValidator> validator_;
    std::unique_ptr<SecureClipboard> clipboard_;
};

// Input Validation Tests

TEST_F(EditorSecurityTest, ValidateCharacter_NormalChars) {
    // Test normal ASCII characters
    auto result = validator_->validateCharacter('a', 10);
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 'a');

    result = validator_->validateCharacter('Z', 10);
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 'Z');

    result = validator_->validateCharacter('5', 10);
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), '5');
}

TEST_F(EditorSecurityTest, ValidateCharacter_ControlChars) {
    // Test allowed control characters
    auto result = validator_->validateCharacter('\n', 10);
    EXPECT_TRUE(result.has_value());

    result = validator_->validateCharacter('\t', 10);
    EXPECT_TRUE(result.has_value());

    result = validator_->validateCharacter('\r', 10);
    EXPECT_TRUE(result.has_value());

    // Test disallowed control characters
    result = validator_->validateCharacter('\x01', 10); // SOH
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code(), ErrorCode::kValidationError);
}

TEST_F(EditorSecurityTest, ValidateCharacter_DangerousEscapes) {
    // Test dangerous escape sequence characters
    auto result = validator_->validateCharacter('\x1b', 10); // ESC
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code(), ErrorCode::kSecurityError);

    result = validator_->validateCharacter('\x07', 10); // BEL
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code(), ErrorCode::kSecurityError);

    result = validator_->validateCharacter('\x7f', 10); // DEL
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code(), ErrorCode::kSecurityError);
}

TEST_F(EditorSecurityTest, ValidateCharacter_LineLengthLimit) {
    EditorInputValidator::ValidationConfig config;
    config.max_line_length = 5;
    EditorInputValidator limited_validator(config);

    // Should succeed under limit
    auto result = limited_validator.validateCharacter('a', 4);
    EXPECT_TRUE(result.has_value());

    // Should fail at limit
    result = limited_validator.validateCharacter('a', 5);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code(), ErrorCode::kValidationError);
}

TEST_F(EditorSecurityTest, ValidateString_TerminalInjection) {
    // Test various terminal injection attempts
    std::vector<std::string> injection_attempts = {
        "\x1b[2J",           // Clear screen
        "\x1b[H",            // Cursor home
        "\x1b]0;title\x07",  // Set window title
        "\x1b[?1049h",       // Enable alternate screen
        "\x1b[31m",          // Set color
        "normal\x1b[2Jtext", // Mixed content with injection
    };

    for (const auto& attempt : injection_attempts) {
        auto result = validator_->validateString(attempt, 0);
        // Should either fail or be sanitized
        if (result.has_value()) {
            // If it succeeds, dangerous characters should be sanitized
            const std::string& sanitized = result.value();
            EXPECT_EQ(sanitized.find('\x1b'), std::string::npos) 
                << "ESC character not removed from: " << attempt;
        } else {
            // Rejection is also acceptable
            EXPECT_EQ(result.error().code(), ErrorCode::kSecurityError);
        }
    }
}

TEST_F(EditorSecurityTest, ValidateString_SizeLimit) {
    EditorInputValidator::ValidationConfig config;
    config.max_total_size = 100;
    EditorInputValidator limited_validator(config);

    std::string large_input(150, 'a');
    auto result = limited_validator.validateString(large_input, 0);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code(), ErrorCode::kValidationError);

    // With existing content
    std::string small_input(50, 'b');
    result = limited_validator.validateString(small_input, 60);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code(), ErrorCode::kValidationError);
}

TEST_F(EditorSecurityTest, ValidateUtf8Sequence_ValidSequences) {
    // Valid UTF-8 sequences
    std::vector<std::vector<uint8_t>> valid_sequences = {
        {0x41},                    // ASCII 'A'
        {0xC3, 0xA9},             // é (Latin small letter e with acute)
        {0xE2, 0x82, 0xAC},       // € (Euro sign)
        {0xF0, 0x9F, 0x98, 0x80}, // 😀 (grinning face emoji)
    };

    for (const auto& sequence : valid_sequences) {
        auto result = validator_->validateUtf8Sequence(sequence);
        EXPECT_TRUE(result.has_value()) 
            << "Valid UTF-8 sequence rejected";
    }
}

TEST_F(EditorSecurityTest, ValidateUtf8Sequence_InvalidSequences) {
    // Invalid UTF-8 sequences
    std::vector<std::vector<uint8_t>> invalid_sequences = {
        {0xC0, 0x80},             // Overlong encoding of NULL
        {0xE0, 0x80, 0x80},       // Overlong encoding
        {0xF8, 0x80, 0x80, 0x80}, // Invalid start byte
        {0x80},                   // Standalone continuation byte
        {0xC3},                   // Incomplete sequence
        {0xFF},                   // Invalid byte
    };

    for (const auto& sequence : invalid_sequences) {
        auto result = validator_->validateUtf8Sequence(sequence);
        EXPECT_FALSE(result.has_value()) 
            << "Invalid UTF-8 sequence accepted";
        EXPECT_EQ(result.error().code(), ErrorCode::kValidationError);
    }
}

TEST_F(EditorSecurityTest, SanitizeInput_RemovesDangerousChars) {
    std::string input = "Hello\x1b[2JWorld\x07Test";
    std::string sanitized = validator_->sanitizeInput(input);
    
    // Should not contain dangerous characters
    EXPECT_EQ(sanitized.find('\x1b'), std::string::npos);
    EXPECT_EQ(sanitized.find('\x07'), std::string::npos);
    
    // Should preserve normal characters
    EXPECT_NE(sanitized.find("Hello"), std::string::npos);
    EXPECT_NE(sanitized.find("World"), std::string::npos);
    EXPECT_NE(sanitized.find("Test"), std::string::npos);
}

// Bounds Checking Tests

TEST_F(EditorSecurityTest, ValidateLineIndex_ValidIndices) {
    auto result = EditorBoundsChecker::validateLineIndex(0, 5);
    EXPECT_TRUE(result.has_value());

    result = EditorBoundsChecker::validateLineIndex(4, 5);
    EXPECT_TRUE(result.has_value());
}

TEST_F(EditorSecurityTest, ValidateLineIndex_InvalidIndices) {
    auto result = EditorBoundsChecker::validateLineIndex(5, 5);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code(), ErrorCode::kValidationError);

    result = EditorBoundsChecker::validateLineIndex(0, 0);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code(), ErrorCode::kValidationError);
}

TEST_F(EditorSecurityTest, ValidateColumnIndex_ValidIndices) {
    auto result = EditorBoundsChecker::validateColumnIndex(0, 10);
    EXPECT_TRUE(result.has_value());

    result = EditorBoundsChecker::validateColumnIndex(10, 10);
    EXPECT_TRUE(result.has_value());
}

TEST_F(EditorSecurityTest, ValidateColumnIndex_InvalidIndices) {
    auto result = EditorBoundsChecker::validateColumnIndex(11, 10);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code(), ErrorCode::kValidationError);
}

TEST_F(EditorSecurityTest, ValidateCursorPosition_ClampsToValidRange) {
    std::vector<std::string> lines = {"Hello", "World", "Test"};
    
    // Valid position
    auto result = EditorBoundsChecker::validateCursorPosition(1, 3, lines);
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value().first, 1);
    EXPECT_EQ(result.value().second, 3);

    // Line out of bounds - should clamp
    result = EditorBoundsChecker::validateCursorPosition(5, 0, lines);
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value().first, 2); // Clamped to last line

    // Column out of bounds - should clamp
    result = EditorBoundsChecker::validateCursorPosition(0, 10, lines);
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value().first, 0);
    EXPECT_EQ(result.value().second, 5); // Clamped to line length
}

TEST_F(EditorSecurityTest, ValidateMemoryUsage_ChecksLimits) {
    auto result = EditorBoundsChecker::validateMemoryUsage(50, 30, 100);
    EXPECT_TRUE(result.has_value());

    result = EditorBoundsChecker::validateMemoryUsage(50, 60, 100);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code(), ErrorCode::kValidationError);

    result = EditorBoundsChecker::validateMemoryUsage(150, 10, 100);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code(), ErrorCode::kValidationError);
}

TEST_F(EditorSecurityTest, SafeSubstring_HandlesEdgeCases) {
    std::string text = "Hello World";
    
    // Normal case
    auto result = EditorBoundsChecker::safeSubstring(text, 0, 5);
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "Hello");

    // Start beyond end
    result = EditorBoundsChecker::safeSubstring(text, 20, 5);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code(), ErrorCode::kValidationError);

    // Length extends beyond end - should clamp
    result = EditorBoundsChecker::safeSubstring(text, 6, 20);
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "World");
}

// Secure Clipboard Tests

TEST_F(EditorSecurityTest, SecureClipboard_BasicOperations) {
    // Initially empty
    EXPECT_FALSE(clipboard_->hasContent());
    EXPECT_EQ(clipboard_->getContentSize(), 0);

    // Set content
    std::string test_content = "Test clipboard content";
    auto result = clipboard_->setContent(test_content);
    EXPECT_TRUE(result.has_value());
    EXPECT_TRUE(clipboard_->hasContent());
    EXPECT_EQ(clipboard_->getContentSize(), test_content.length());

    // Get content
    auto content_result = clipboard_->getContent();
    EXPECT_TRUE(content_result.has_value());
    EXPECT_EQ(content_result.value(), test_content);

    // Clear
    clipboard_->clear();
    EXPECT_FALSE(clipboard_->hasContent());
    EXPECT_EQ(clipboard_->getContentSize(), 0);
}

TEST_F(EditorSecurityTest, SecureClipboard_EmptyContent) {
    auto result = clipboard_->getContent();
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code(), ErrorCode::kNotFound);
}

TEST_F(EditorSecurityTest, SecureClipboard_OverwritesContent) {
    clipboard_->setContent("First content");
    clipboard_->setContent("Second content");

    auto result = clipboard_->getContent();
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "Second content");
}

// Fuzzing-style Tests

TEST_F(EditorSecurityTest, FuzzTest_RandomInputValidation) {
    // Generate semi-random inputs to test robustness
    std::vector<std::string> fuzz_inputs = {
        std::string(1000, '\x00'),         // Null bytes
        std::string(1000, '\xFF'),         // Invalid UTF-8
        std::string(500, '\x1b') + "test", // Many escape characters
        "Normal\x00text\x1b[2J",          // Mixed content
        "",                               // Empty string
        std::string(1, '\x80'),           // Single continuation byte
    };

    for (const auto& input : fuzz_inputs) {
        // Should not crash or cause undefined behavior
        auto result = validator_->validateString(input, 0);
        // Either succeeds with sanitized input or fails safely
        if (!result.has_value()) {
            EXPECT_NE(result.error().code(), ErrorCode::kSuccess);
        }
    }
}

TEST_F(EditorSecurityTest, StressTest_LargeInputs) {
    // Test with large inputs to check performance and memory safety
    EditorInputValidator::ValidationConfig config;
    config.max_total_size = 10 * 1024 * 1024; // 10MB
    EditorInputValidator stress_validator(config);

    std::string large_input(1024 * 1024, 'a'); // 1MB of 'a's
    auto result = stress_validator.validateString(large_input, 0);
    EXPECT_TRUE(result.has_value());

    // Should handle at size limit
    std::string max_input(10 * 1024 * 1024 - 1, 'b');
    result = stress_validator.validateString(max_input, 1);
    EXPECT_TRUE(result.has_value());
}

// Integration Tests

TEST_F(EditorSecurityTest, Integration_ValidateAndSanitizePipeline) {
    std::string dangerous_input = "Hello\x1b[2J\x07World";
    
    // First validate
    auto validate_result = validator_->validateString(dangerous_input, 0);
    EXPECT_TRUE(validate_result.has_value());
    
    // Result should be sanitized
    std::string sanitized = validate_result.value();
    EXPECT_EQ(sanitized.find('\x1b'), std::string::npos);
    EXPECT_EQ(sanitized.find('\x07'), std::string::npos);
    
    // Should still contain safe content
    EXPECT_NE(sanitized.find("Hello"), std::string::npos);
    EXPECT_NE(sanitized.find("World"), std::string::npos);
}

TEST_F(EditorSecurityTest, Performance_ValidationSpeed) {
    std::string test_input(10000, 'a'); // 10KB of data
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < 100; ++i) {
        auto result = validator_->validateString(test_input, 0);
        EXPECT_TRUE(result.has_value());
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // Should validate 1MB (100 * 10KB) in reasonable time
    EXPECT_LT(duration.count(), 1000) << "Validation too slow: " << duration.count() << "ms";
}