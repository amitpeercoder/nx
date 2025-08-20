#include "nx/tui/unicode_handler.hpp"
#include <algorithm>
#include <cstring>
#include <unicode/uclean.h>
#include <unicode/uloc.h>

namespace nx::tui {

// Static member initialization
bool UnicodeHandler::initialized_ = false;
UBreakIterator* UnicodeHandler::word_break_iter_ = nullptr;
const UNormalizer2* UnicodeHandler::normalizer_ = nullptr;

Result<void> UnicodeHandler::initialize(const std::string& locale) {
    if (initialized_) {
        return {}; // Already initialized
    }

    UErrorCode status = U_ZERO_ERROR;

    // Set locale if provided
    if (!locale.empty()) {
        uloc_setDefault(locale.c_str(), &status);
        if (U_FAILURE(status)) {
            return makeErrorResult<void>(ErrorCode::kSystemError, 
                "Failed to set Unicode locale: " + std::string(u_errorName(status)));
        }
    }

    // Initialize break iterator
    auto break_result = initializeBreakIterator();
    if (!break_result) {
        return break_result;
    }

    // Initialize normalizer
    normalizer_ = unorm2_getNFCInstance(&status);
    if (U_FAILURE(status) || !normalizer_) {
        cleanupBreakIterator();
        return makeErrorResult<void>(ErrorCode::kSystemError, 
            "Failed to initialize Unicode normalizer: " + std::string(u_errorName(status)));
    }

    initialized_ = true;
    return {};
}

void UnicodeHandler::cleanup() {
    if (!initialized_) {
        return;
    }

    cleanupBreakIterator();
    normalizer_ = nullptr;
    
    // Cleanup ICU resources
    u_cleanup();
    
    initialized_ = false;
}

size_t UnicodeHandler::calculateDisplayWidth(const std::string& text) {
    if (text.empty()) {
        return 0;
    }

    size_t total_width = 0;
    Utf8Iterator iter(text);
    
    while (iter.hasNext()) {
        UChar32 codepoint = iter.next();
        total_width += getCodePointWidth(codepoint);
    }

    return total_width;
}

size_t UnicodeHandler::getCodePointWidth(UChar32 codepoint) {
    // Control characters have zero width
    if (u_iscntrl(codepoint)) {
        return 0;
    }

    // Combining marks have zero width
    if (isCombiningMark(codepoint)) {
        return 0;
    }

    // East Asian Wide and Fullwidth characters have width 2
    int8_t east_asian_width = u_getIntPropertyValue(codepoint, UCHAR_EAST_ASIAN_WIDTH);
    if (east_asian_width == U_EA_FULLWIDTH || east_asian_width == U_EA_WIDE) {
        return 2;
    }

    // Most characters have width 1
    return 1;
}

Result<std::u16string> UnicodeHandler::utf8ToUtf16(const std::string& utf8_text) {
    if (utf8_text.empty()) {
        return std::u16string();
    }

    UErrorCode status = U_ZERO_ERROR;
    
    // Calculate required buffer size
    int32_t utf16_length = 0;
    u_strFromUTF8(nullptr, 0, &utf16_length, utf8_text.c_str(), utf8_text.length(), &status);
    
    if (status != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(status)) {
        return makeErrorResult<std::u16string>(ErrorCode::kValidationError, 
            "Failed to calculate UTF-16 length: " + std::string(u_errorName(status)));
    }

    // Allocate buffer and convert
    std::u16string result(utf16_length, 0);
    status = U_ZERO_ERROR;
    
    u_strFromUTF8(reinterpret_cast<UChar*>(result.data()), utf16_length, nullptr,
                  utf8_text.c_str(), utf8_text.length(), &status);
    
    if (U_FAILURE(status)) {
        return makeErrorResult<std::u16string>(ErrorCode::kValidationError, 
            "Failed to convert UTF-8 to UTF-16: " + std::string(u_errorName(status)));
    }

    return result;
}

Result<std::string> UnicodeHandler::utf16ToUtf8(const std::u16string& utf16_text) {
    if (utf16_text.empty()) {
        return std::string();
    }

    UErrorCode status = U_ZERO_ERROR;
    
    // Calculate required buffer size
    int32_t utf8_length = 0;
    u_strToUTF8(nullptr, 0, &utf8_length, 
                reinterpret_cast<const UChar*>(utf16_text.c_str()), utf16_text.length(), &status);
    
    if (status != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(status)) {
        return makeErrorResult<std::string>(ErrorCode::kValidationError, 
            "Failed to calculate UTF-8 length: " + std::string(u_errorName(status)));
    }

    // Allocate buffer and convert
    std::string result(utf8_length, 0);
    status = U_ZERO_ERROR;
    
    u_strToUTF8(result.data(), utf8_length, nullptr,
                reinterpret_cast<const UChar*>(utf16_text.c_str()), utf16_text.length(), &status);
    
    if (U_FAILURE(status)) {
        return makeErrorResult<std::string>(ErrorCode::kValidationError, 
            "Failed to convert UTF-16 to UTF-8: " + std::string(u_errorName(status)));
    }

    return result;
}

Result<size_t> UnicodeHandler::displayColumnToCharIndex(const std::string& text, size_t display_col) {
    if (display_col == 0) {
        return 0;
    }

    size_t current_col = 0;
    size_t char_index = 0;
    Utf8Iterator iter(text);
    
    while (iter.hasNext() && current_col < display_col) {
        UChar32 codepoint = iter.next();
        current_col += getCodePointWidth(codepoint);
        char_index++;
    }

    return char_index;
}

Result<size_t> UnicodeHandler::charIndexToDisplayColumn(const std::string& text, size_t char_index) {
    if (char_index == 0) {
        return 0;
    }

    size_t current_col = 0;
    size_t current_char = 0;
    Utf8Iterator iter(text);
    
    while (iter.hasNext() && current_char < char_index) {
        UChar32 codepoint = iter.next();
        current_col += getCodePointWidth(codepoint);
        current_char++;
    }

    return current_col;
}

Result<size_t> UnicodeHandler::findNextWordBoundary(const std::string& text, size_t current_pos) {
    if (!initialized_ || !word_break_iter_) {
        return makeErrorResult<size_t>(ErrorCode::kInvalidState, "Unicode handler not initialized");
    }

    // Convert to UTF-16 for ICU processing
    auto utf16_result = utf8ToUtf16(text);
    if (!utf16_result) {
        return std::unexpected(utf16_result.error());
    }

    UErrorCode status = U_ZERO_ERROR;
    ubrk_setText(word_break_iter_, 
                 reinterpret_cast<const UChar*>(utf16_result.value().c_str()),
                 utf16_result.value().length(), &status);
    
    if (U_FAILURE(status)) {
        return makeErrorResult<size_t>(ErrorCode::kSystemError, 
            "Failed to set break iterator text: " + std::string(u_errorName(status)));
    }

    // Convert position to UTF-16 offset
    Utf8Iterator iter(text);
    size_t utf16_pos = 0;
    for (size_t i = 0; i < current_pos && iter.hasNext(); ++i) {
        iter.next();
        utf16_pos++;
    }

    int32_t next_boundary = ubrk_following(word_break_iter_, utf16_pos);
    if (next_boundary == UBRK_DONE) {
        return text.length(); // End of text
    }

    // Convert back to UTF-8 character index
    return std::min(static_cast<size_t>(next_boundary), text.length());
}

Result<size_t> UnicodeHandler::findPreviousWordBoundary(const std::string& text, size_t current_pos) {
    if (!initialized_ || !word_break_iter_) {
        return makeErrorResult<size_t>(ErrorCode::kInvalidState, "Unicode handler not initialized");
    }

    if (current_pos == 0) {
        return 0;
    }

    // Convert to UTF-16 for ICU processing
    auto utf16_result = utf8ToUtf16(text);
    if (!utf16_result) {
        return std::unexpected(utf16_result.error());
    }

    UErrorCode status = U_ZERO_ERROR;
    ubrk_setText(word_break_iter_, 
                 reinterpret_cast<const UChar*>(utf16_result.value().c_str()),
                 utf16_result.value().length(), &status);
    
    if (U_FAILURE(status)) {
        return makeErrorResult<size_t>(ErrorCode::kSystemError, 
            "Failed to set break iterator text: " + std::string(u_errorName(status)));
    }

    // Convert position to UTF-16 offset
    Utf8Iterator iter(text);
    size_t utf16_pos = 0;
    for (size_t i = 0; i < current_pos && iter.hasNext(); ++i) {
        iter.next();
        utf16_pos++;
    }

    int32_t prev_boundary = ubrk_preceding(word_break_iter_, utf16_pos);
    if (prev_boundary == UBRK_DONE) {
        return 0; // Beginning of text
    }

    return static_cast<size_t>(prev_boundary);
}

Result<void> UnicodeHandler::validateUtf8(const std::string& text) {
    if (text.empty()) {
        return {};
    }

    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(text.c_str());
    size_t length = text.length();
    size_t i = 0;

    while (i < length) {
        uint8_t first_byte = bytes[i];
        size_t char_length = getUtf8CharLength(first_byte);

        if (char_length == 0) {
            return makeErrorResult<void>(ErrorCode::kValidationError, 
                "Invalid UTF-8 start byte at position " + std::to_string(i));
        }

        if (i + char_length > length) {
            return makeErrorResult<void>(ErrorCode::kValidationError, 
                "Incomplete UTF-8 sequence at position " + std::to_string(i));
        }

        if (!isValidUtf8Sequence(bytes + i, char_length)) {
            return makeErrorResult<void>(ErrorCode::kValidationError, 
                "Invalid UTF-8 sequence at position " + std::to_string(i));
        }

        i += char_length;
    }

    return {};
}

Result<std::string> UnicodeHandler::normalizeText(const std::string& text) {
    if (!initialized_ || !normalizer_) {
        return makeErrorResult<std::string>(ErrorCode::kInvalidState, "Unicode handler not initialized");
    }

    if (text.empty()) {
        return text;
    }

    // Convert to UTF-16
    auto utf16_result = utf8ToUtf16(text);
    if (!utf16_result) {
        return std::unexpected(utf16_result.error());
    }

    UErrorCode status = U_ZERO_ERROR;
    const std::u16string& input = utf16_result.value();
    
    // Calculate normalized length
    int32_t norm_length = unorm2_normalize(normalizer_, 
                                          reinterpret_cast<const UChar*>(input.c_str()),
                                          input.length(), nullptr, 0, &status);
    
    if (status != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(status)) {
        return makeErrorResult<std::string>(ErrorCode::kSystemError, 
            "Failed to calculate normalized length: " + std::string(u_errorName(status)));
    }

    // Normalize text
    std::u16string normalized(norm_length, 0);
    status = U_ZERO_ERROR;
    
    unorm2_normalize(normalizer_, 
                     reinterpret_cast<const UChar*>(input.c_str()),
                     input.length(),
                     reinterpret_cast<UChar*>(normalized.data()),
                     norm_length, &status);
    
    if (U_FAILURE(status)) {
        return makeErrorResult<std::string>(ErrorCode::kSystemError, 
            "Failed to normalize text: " + std::string(u_errorName(status)));
    }

    // Convert back to UTF-8
    return utf16ToUtf8(normalized);
}

bool UnicodeHandler::isCombiningMark(UChar32 codepoint) {
    int8_t category = u_charType(codepoint);
    return category == U_NON_SPACING_MARK || 
           category == U_ENCLOSING_MARK || 
           category == U_COMBINING_SPACING_MARK;
}

bool UnicodeHandler::isLineSeparator(UChar32 codepoint) {
    return codepoint == 0x000A ||  // Line Feed
           codepoint == 0x000D ||  // Carriage Return
           codepoint == 0x0085 ||  // Next Line
           codepoint == 0x2028 ||  // Line Separator
           codepoint == 0x2029;    // Paragraph Separator
}

bool UnicodeHandler::isWhitespace(UChar32 codepoint) {
    return u_isUWhiteSpace(codepoint);
}

int8_t UnicodeHandler::getCharacterCategory(UChar32 codepoint) {
    return u_charType(codepoint);
}

UnicodeHandler::TextMetrics UnicodeHandler::analyzeText(const std::string& text) {
    TextMetrics metrics = {};
    
    if (text.empty()) {
        metrics.line_count = 1; // Empty text still represents one line
        return metrics;
    }

    metrics.byte_length = text.length();
    
    Utf8Iterator iter(text);
    while (iter.hasNext()) {
        UChar32 codepoint = iter.next();
        
        metrics.character_count++;
        metrics.display_width += getCodePointWidth(codepoint);
        
        if (codepoint == '\n') {
            metrics.line_count++;
        }
        
        if (isCombiningMark(codepoint)) {
            metrics.contains_combining = true;
        }
        
        // Check for RTL characters
        int8_t direction = u_charDirection(codepoint);
        if (direction == U_RIGHT_TO_LEFT || direction == U_RIGHT_TO_LEFT_ARABIC) {
            metrics.contains_rtl = true;
        }
    }
    
    // If no newlines found, there's still one line
    if (metrics.line_count == 0) {
        metrics.line_count = 1;
    }

    return metrics;
}

Result<std::string> UnicodeHandler::truncateToWidth(const std::string& text, size_t max_width, bool ellipsis) {
    if (text.empty() || max_width == 0) {
        return std::string();
    }

    size_t current_width = 0;
    std::string result;
    Utf8Iterator iter(text);
    
    while (iter.hasNext()) {
        size_t char_start = iter.getIndex();
        UChar32 codepoint = iter.next();
        size_t char_width = getCodePointWidth(codepoint);
        
        // Check if adding this character would exceed limit
        // If using ellipsis, reserve 3 characters for "..."
        size_t effective_max_width = ellipsis ? (max_width >= 3 ? max_width - 3 : 0) : max_width;
        
        if (current_width + char_width > effective_max_width) {
            if (ellipsis && max_width >= 3) {
                result += "...";
            }
            break;
        }
        
        // Add the character bytes to result
        size_t char_end = iter.getIndex();
        result += text.substr(char_start, char_end - char_start);
        current_width += char_width;
    }

    return result;
}

// Private helper functions

Result<void> UnicodeHandler::initializeBreakIterator() {
    UErrorCode status = U_ZERO_ERROR;
    word_break_iter_ = ubrk_open(UBRK_WORD, nullptr, nullptr, 0, &status);
    
    if (U_FAILURE(status) || !word_break_iter_) {
        return makeErrorResult<void>(ErrorCode::kSystemError, 
            "Failed to create word break iterator: " + std::string(u_errorName(status)));
    }

    return {};
}

void UnicodeHandler::cleanupBreakIterator() {
    if (word_break_iter_) {
        ubrk_close(word_break_iter_);
        word_break_iter_ = nullptr;
    }
}

size_t UnicodeHandler::getUtf8CharLength(uint8_t first_byte) {
    if ((first_byte & 0x80) == 0) return 1;      // 0xxxxxxx
    if ((first_byte & 0xe0) == 0xc0) return 2;   // 110xxxxx
    if ((first_byte & 0xf0) == 0xe0) return 3;   // 1110xxxx
    if ((first_byte & 0xf8) == 0xf0) return 4;   // 11110xxx
    return 0; // Invalid
}

bool UnicodeHandler::isValidUtf8Sequence(const uint8_t* bytes, size_t length) {
    if (length == 0) return false;
    
    uint8_t first = bytes[0];
    
    // ASCII
    if (length == 1 && (first & 0x80) == 0) {
        return true;
    }
    
    // Multi-byte sequences
    for (size_t i = 1; i < length; ++i) {
        if ((bytes[i] & 0xc0) != 0x80) {
            return false; // Invalid continuation byte
        }
    }
    
    // Check specific patterns
    if (length == 2 && (first & 0xe0) == 0xc0) {
        return (first & 0x1f) >= 0x02; // No overlong encoding
    }
    if (length == 3 && (first & 0xf0) == 0xe0) {
        return !(first == 0xe0 && (bytes[1] & 0xe0) == 0x80); // No overlong
    }
    if (length == 4 && (first & 0xf8) == 0xf0) {
        return !(first == 0xf0 && (bytes[1] & 0xf0) == 0x80); // No overlong
    }
    
    return false;
}

Result<UChar32> UnicodeHandler::getCodePointAt(const std::string& text, size_t& index) {
    if (index >= text.length()) {
        return makeErrorResult<UChar32>(ErrorCode::kValidationError, "Index out of bounds");
    }
    
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(text.data());
    uint8_t first_byte = bytes[index];
    
    // Determine character length
    size_t char_length = getUtf8CharLength(first_byte);
    if (char_length == 0 || index + char_length > text.length()) {
        return makeErrorResult<UChar32>(ErrorCode::kValidationError, "Invalid UTF-8 sequence");
    }
    
    // Validate the sequence
    if (!isValidUtf8Sequence(bytes + index, char_length)) {
        return makeErrorResult<UChar32>(ErrorCode::kValidationError, "Invalid UTF-8 sequence");
    }
    
    UChar32 codepoint = 0;
    
    switch (char_length) {
        case 1:
            codepoint = first_byte;
            break;
        case 2:
            codepoint = ((first_byte & 0x1f) << 6) | (bytes[index + 1] & 0x3f);
            break;
        case 3:
            codepoint = ((first_byte & 0x0f) << 12) | 
                       ((bytes[index + 1] & 0x3f) << 6) | 
                       (bytes[index + 2] & 0x3f);
            break;
        case 4:
            codepoint = ((first_byte & 0x07) << 18) | 
                       ((bytes[index + 1] & 0x3f) << 12) | 
                       ((bytes[index + 2] & 0x3f) << 6) | 
                       (bytes[index + 3] & 0x3f);
            break;
        default:
            return makeErrorResult<UChar32>(ErrorCode::kValidationError, "Invalid character length");
    }
    
    // Update index to next character
    index += char_length;
    
    return codepoint;
}

// Utf8Iterator Implementation

UnicodeHandler::Utf8Iterator::Utf8Iterator(const std::string& text) 
    : text_(text), index_(0), length_(text.length()) {
}

bool UnicodeHandler::Utf8Iterator::hasNext() const {
    return index_ < length_;
}

UChar32 UnicodeHandler::Utf8Iterator::next() {
    if (!hasNext()) {
        return U_SENTINEL; // End of iteration
    }

    UChar32 codepoint = U_SENTINEL;
    int32_t idx = static_cast<int32_t>(index_);
    int32_t len = static_cast<int32_t>(length_);
    
    U8_NEXT(text_.c_str(), idx, len, codepoint);
    
    // If ICU failed, try simple ASCII fallback
    if (codepoint == U_SENTINEL && index_ < length_) {
        uint8_t byte = static_cast<uint8_t>(text_[index_]);
        if (byte < 0x80) {  // ASCII
            codepoint = static_cast<UChar32>(byte);
            index_++;
            return codepoint;
        } else {
            // Skip invalid byte
            index_++;
            return U_SENTINEL;
        }
    }
    
    index_ = static_cast<size_t>(idx);
    return codepoint;
}

UChar32 UnicodeHandler::Utf8Iterator::previous() {
    if (index_ == 0) {
        return U_SENTINEL; // Beginning of iteration
    }

    UChar32 codepoint = U_SENTINEL;
    int32_t idx = static_cast<int32_t>(index_);
    U8_PREV(text_.c_str(), 0, idx, codepoint);
    index_ = static_cast<size_t>(idx);
    
    return codepoint;
}

size_t UnicodeHandler::Utf8Iterator::getIndex() const {
    return index_;
}

void UnicodeHandler::Utf8Iterator::setIndex(size_t index) {
    index_ = std::min(index, length_);
}

size_t UnicodeHandler::Utf8Iterator::length() const {
    return length_;
}

// UnicodeCursor Implementation

UnicodeCursor::UnicodeCursor(const std::string& text) 
    : text_(text), char_index_(0), byte_index_(0), display_column_(0), cache_valid_(false) {
    buildPositionCache();
}

Result<void> UnicodeCursor::setPosition(size_t char_index) {
    if (!cache_valid_) {
        buildPositionCache();
    }
    
    if (char_index >= char_to_byte_map_.size()) {
        return makeErrorResult<void>(ErrorCode::kValidationError, 
            "Character index out of bounds");
    }
    
    char_index_ = char_index;
    byte_index_ = char_to_byte_map_[char_index];
    display_column_ = char_to_column_map_[char_index];
    
    return {};
}

Result<void> UnicodeCursor::moveForward() {
    if (char_index_ >= char_to_byte_map_.size() - 1) {
        return makeErrorResult<void>(ErrorCode::kValidationError, 
            "Already at end of text");
    }
    
    char_index_++;
    byte_index_ = char_to_byte_map_[char_index_];
    display_column_ = char_to_column_map_[char_index_];
    
    return {};
}

Result<void> UnicodeCursor::moveBackward() {
    if (char_index_ == 0) {
        return makeErrorResult<void>(ErrorCode::kValidationError, 
            "Already at start of text");
    }
    
    char_index_--;
    byte_index_ = char_to_byte_map_[char_index_];
    display_column_ = char_to_column_map_[char_index_];
    
    return {};
}

Result<void> UnicodeCursor::moveToNextWord() {
    auto next_boundary = UnicodeHandler::findNextWordBoundary(text_, char_index_);
    if (!next_boundary) {
        return std::unexpected(next_boundary.error());
    }
    
    return setPosition(next_boundary.value());
}

Result<void> UnicodeCursor::moveToPreviousWord() {
    auto prev_boundary = UnicodeHandler::findPreviousWordBoundary(text_, char_index_);
    if (!prev_boundary) {
        return std::unexpected(prev_boundary.error());
    }
    
    return setPosition(prev_boundary.value());
}

size_t UnicodeCursor::getCharacterIndex() const {
    return char_index_;
}

size_t UnicodeCursor::getByteIndex() const {
    return byte_index_;
}

size_t UnicodeCursor::getDisplayColumn() const {
    return display_column_;
}

Result<void> UnicodeCursor::moveToDisplayColumn(size_t column) {
    if (!cache_valid_) {
        buildPositionCache();
    }
    
    // Find character index that corresponds to the closest display column
    auto it = std::lower_bound(char_to_column_map_.begin(), char_to_column_map_.end(), column);
    if (it == char_to_column_map_.end()) {
        // Move to end of text
        return setPosition(char_to_column_map_.size() - 1);
    }
    
    size_t char_index = std::distance(char_to_column_map_.begin(), it);
    return setPosition(char_index);
}

UChar32 UnicodeCursor::getCurrentCharacter() const {
    if (byte_index_ >= text_.length()) {
        return 0; // End of text
    }
    
    size_t temp_index = byte_index_;
    auto result = UnicodeHandler::getCodePointAt(text_, temp_index);
    return result.value_or(0);
}

void UnicodeCursor::updateText(const std::string& new_text) {
    text_ = new_text;
    cache_valid_ = false;
    char_index_ = 0;
    byte_index_ = 0;
    display_column_ = 0;
    buildPositionCache();
}

void UnicodeCursor::buildPositionCache() {
    char_to_byte_map_.clear();
    char_to_column_map_.clear();
    
    size_t byte_pos = 0;
    size_t display_col = 0;
    
    // Always include position 0
    char_to_byte_map_.push_back(0);
    char_to_column_map_.push_back(0);
    
    while (byte_pos < text_.length()) {
        auto codepoint_result = UnicodeHandler::getCodePointAt(text_, byte_pos);
        
        if (!codepoint_result) {
            break; // Invalid UTF-8
        }
        
        UChar32 codepoint = codepoint_result.value();
        
        // Calculate display width for this character
        size_t char_width = UnicodeHandler::getCodePointWidth(codepoint);
        display_col += char_width;
        
        // Record positions for this character
        char_to_byte_map_.push_back(byte_pos);
        char_to_column_map_.push_back(display_col);
    }
    
    cache_valid_ = true;
}

void UnicodeCursor::rebuildCache() {
    cache_valid_ = false;
    buildPositionCache();
}

void UnicodeCursor::invalidateCache() {
    cache_valid_ = false;
}

Result<void> UnicodeCursor::validatePosition() {
    if (!cache_valid_) {
        buildPositionCache();
    }
    
    if (char_index_ >= char_to_byte_map_.size()) {
        return makeErrorResult<void>(ErrorCode::kValidationError, 
            "Character index out of bounds");
    }
    
    if (byte_index_ >= text_.length()) {
        return makeErrorResult<void>(ErrorCode::kValidationError, 
            "Byte index out of bounds");
    }
    
    return {};
}

} // namespace nx::tui