#include <gtest/gtest.h>
#include "nx/tui/markdown_highlighter.hpp"

using namespace nx::tui;

class MarkdownHighlighterTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_ = MarkdownHighlightConfig{};
        highlighter_ = std::make_unique<MarkdownHighlighter>(config_);
    }
    
    MarkdownHighlightConfig config_;
    std::unique_ptr<MarkdownHighlighter> highlighter_;
};

// Header Tests

TEST_F(MarkdownHighlighterTest, HighlightHeaders) {
    auto result = highlighter_->highlightLine("# Heading 1");
    if (!result.success) {
        std::cout << "Error message: " << result.error_message << std::endl;
    }
    EXPECT_TRUE(result.success);
    EXPECT_GE(result.segments.size(), 1);
    
    // Should have header text segment
    bool found_header = false;
    for (const auto& seg : result.segments) {
        if (seg.element_type == "header_text") {
            found_header = true;
            EXPECT_EQ(seg.start_pos, 2);  // After "# "
            EXPECT_EQ(seg.end_pos, 11);   // End of "Heading 1"
            break;
        }
    }
    EXPECT_TRUE(found_header);
}

TEST_F(MarkdownHighlighterTest, HighlightMultipleLevelHeaders) {
    auto result1 = highlighter_->highlightLine("## Heading 2");
    auto result2 = highlighter_->highlightLine("### Heading 3");
    auto result3 = highlighter_->highlightLine("###### Heading 6");
    
    EXPECT_TRUE(result1.success);
    EXPECT_TRUE(result2.success);
    EXPECT_TRUE(result3.success);
    
    EXPECT_GE(result1.segments.size(), 1);
    EXPECT_GE(result2.segments.size(), 1);
    EXPECT_GE(result3.segments.size(), 1);
}

TEST_F(MarkdownHighlighterTest, IgnoreInvalidHeaders) {
    auto result = highlighter_->highlightLine("####### Too many hashes");
    EXPECT_TRUE(result.success);
    
    // Should not find header segments
    bool found_header = false;
    for (const auto& seg : result.segments) {
        if (seg.element_type == "header_text") {
            found_header = true;
            break;
        }
    }
    EXPECT_FALSE(found_header);
}

// Emphasis Tests

TEST_F(MarkdownHighlighterTest, HighlightItalic) {
    auto result = highlighter_->highlightLine("This is *italic* text");
    EXPECT_TRUE(result.success);
    
    bool found_italic = false;
    for (const auto& seg : result.segments) {
        if (seg.element_type == "italic") {
            found_italic = true;
            EXPECT_EQ(seg.start_pos, 9);   // After "*"
            EXPECT_EQ(seg.end_pos, 15);    // Before "*"
            break;
        }
    }
    EXPECT_TRUE(found_italic);
}

TEST_F(MarkdownHighlighterTest, HighlightBold) {
    auto result = highlighter_->highlightLine("This is **bold** text");
    EXPECT_TRUE(result.success);
    
    bool found_bold = false;
    for (const auto& seg : result.segments) {
        if (seg.element_type == "bold") {
            found_bold = true;
            EXPECT_EQ(seg.start_pos, 10);  // After "**"
            EXPECT_EQ(seg.end_pos, 14);   // Before "**"
            break;
        }
    }
    EXPECT_TRUE(found_bold);
}

TEST_F(MarkdownHighlighterTest, HighlightBoldItalic) {
    auto result = highlighter_->highlightLine("This is ***bold italic*** text");
    EXPECT_TRUE(result.success);
    
    bool found_bold_italic = false;
    for (const auto& seg : result.segments) {
        if (seg.element_type == "bold_italic") {
            found_bold_italic = true;
            EXPECT_EQ(seg.start_pos, 11);  // After "***"
            EXPECT_EQ(seg.end_pos, 22);   // Before "***"
            break;
        }
    }
    EXPECT_TRUE(found_bold_italic);
}

TEST_F(MarkdownHighlighterTest, HighlightUnderscoreEmphasis) {
    auto result = highlighter_->highlightLine("This is _italic_ and __bold__ text");
    EXPECT_TRUE(result.success);
    
    bool found_italic = false;
    bool found_bold = false;
    
    for (const auto& seg : result.segments) {
        if (seg.element_type == "italic" && seg.start_pos == 9) {
            found_italic = true;
        }
        if (seg.element_type == "bold" && seg.start_pos == 23) {
            found_bold = true;
        }
    }
    
    EXPECT_TRUE(found_italic);
    EXPECT_TRUE(found_bold);
}

// Code Tests

TEST_F(MarkdownHighlighterTest, HighlightInlineCode) {
    auto result = highlighter_->highlightLine("This is `inline code` here");
    EXPECT_TRUE(result.success);
    
    bool found_code = false;
    for (const auto& seg : result.segments) {
        if (seg.element_type == "inline_code") {
            found_code = true;
            EXPECT_EQ(seg.start_pos, 8);   // Start of "`inline code`"
            EXPECT_EQ(seg.end_pos, 21);    // End of "`inline code`"
            break;
        }
    }
    EXPECT_TRUE(found_code);
}

TEST_F(MarkdownHighlighterTest, HighlightCodeBlock) {
    auto result = highlighter_->highlightLine("Regular text", 0, true);  // In code block
    EXPECT_TRUE(result.success);
    
    bool found_code_block = false;
    for (const auto& seg : result.segments) {
        if (seg.element_type == "code_block") {
            found_code_block = true;
            EXPECT_EQ(seg.start_pos, 0);
            EXPECT_EQ(seg.end_pos, 12);
            break;
        }
    }
    EXPECT_TRUE(found_code_block);
}

TEST_F(MarkdownHighlighterTest, HighlightMultipleBackticks) {
    auto result = highlighter_->highlightLine("Use ``code with `backticks` `` here");
    EXPECT_TRUE(result.success);
    
    bool found_code = false;
    for (const auto& seg : result.segments) {
        if (seg.element_type == "inline_code") {
            found_code = true;
            break;
        }
    }
    EXPECT_TRUE(found_code);
}

// Link Tests

TEST_F(MarkdownHighlighterTest, HighlightMarkdownLinks) {
    auto result = highlighter_->highlightLine("Check out [this link](https://example.com) for more info");
    EXPECT_TRUE(result.success);
    
    bool found_link_text = false;
    bool found_link_url = false;
    
    for (const auto& seg : result.segments) {
        if (seg.element_type == "link_text") {
            found_link_text = true;
            EXPECT_EQ(seg.start_pos, 11);  // After "["
            EXPECT_EQ(seg.end_pos, 20);   // Before "]"
        }
        if (seg.element_type == "link_url") {
            found_link_url = true;
            EXPECT_EQ(seg.start_pos, 22);  // After "]("
            EXPECT_EQ(seg.end_pos, 41);   // Before ")"
        }
    }
    
    EXPECT_TRUE(found_link_text);
    EXPECT_TRUE(found_link_url);
}

TEST_F(MarkdownHighlighterTest, HighlightAutoLinks) {
    auto result = highlighter_->highlightLine("Visit <https://example.com> for details");
    EXPECT_TRUE(result.success);
    
    bool found_auto_link = false;
    for (const auto& seg : result.segments) {
        if (seg.element_type == "auto_link") {
            found_auto_link = true;
            EXPECT_EQ(seg.start_pos, 6);   // Start of "<https://example.com>"
            EXPECT_EQ(seg.end_pos, 27);    // End of "<https://example.com>"
            break;
        }
    }
    EXPECT_TRUE(found_auto_link);
}

// List Tests

TEST_F(MarkdownHighlighterTest, HighlightUnorderedLists) {
    auto result1 = highlighter_->highlightLine("- First item");
    auto result2 = highlighter_->highlightLine("* Second item");
    auto result3 = highlighter_->highlightLine("+ Third item");
    
    EXPECT_TRUE(result1.success);
    EXPECT_TRUE(result2.success);
    EXPECT_TRUE(result3.success);
    
    for (auto& result : {result1, result2, result3}) {
        bool found_marker = false;
        for (const auto& seg : result.segments) {
            if (seg.element_type == "list_marker") {
                found_marker = true;
                EXPECT_EQ(seg.start_pos, 0);
                EXPECT_EQ(seg.end_pos, 1);
                break;
            }
        }
        EXPECT_TRUE(found_marker);
    }
}

TEST_F(MarkdownHighlighterTest, HighlightOrderedLists) {
    auto result = highlighter_->highlightLine("1. First item");
    EXPECT_TRUE(result.success);
    
    bool found_marker = false;
    for (const auto& seg : result.segments) {
        if (seg.element_type == "ordered_list_marker") {
            found_marker = true;
            EXPECT_EQ(seg.start_pos, 0);
            EXPECT_EQ(seg.end_pos, 2);  // "1."
            break;
        }
    }
    EXPECT_TRUE(found_marker);
}

TEST_F(MarkdownHighlighterTest, HighlightTaskLists) {
    auto result1 = highlighter_->highlightLine("- [ ] Unchecked task");
    auto result2 = highlighter_->highlightLine("- [x] Checked task");
    auto result3 = highlighter_->highlightLine("- [X] Checked task (capital)");
    
    for (auto& result : {result1, result2, result3}) {
        EXPECT_TRUE(result.success);
        
        bool found_marker = false;
        bool found_checkbox = false;
        
        for (const auto& seg : result.segments) {
            if (seg.element_type == "task_list_marker") {
                found_marker = true;
            }
            if (seg.element_type == "task_checkbox") {
                found_checkbox = true;
            }
        }
        
        EXPECT_TRUE(found_marker);
        EXPECT_TRUE(found_checkbox);
    }
}

// Quote Tests

TEST_F(MarkdownHighlighterTest, HighlightBlockquotes) {
    auto result = highlighter_->highlightLine("> This is a quote");
    EXPECT_TRUE(result.success);
    
    bool found_quote = false;
    for (const auto& seg : result.segments) {
        if (seg.element_type == "quote_content") {
            found_quote = true;
            EXPECT_EQ(seg.start_pos, 2);   // After "> "
            EXPECT_EQ(seg.end_pos, 17);    // End of text
            break;
        }
    }
    EXPECT_TRUE(found_quote);
}

TEST_F(MarkdownHighlighterTest, HighlightNestedBlockquotes) {
    auto result = highlighter_->highlightLine(">> Nested quote");
    EXPECT_TRUE(result.success);
    
    bool found_quote = false;
    for (const auto& seg : result.segments) {
        if (seg.element_type == "quote_content") {
            found_quote = true;
            break;
        }
    }
    EXPECT_TRUE(found_quote);
}

// Wiki Link Tests

TEST_F(MarkdownHighlighterTest, HighlightWikiLinks) {
    auto result = highlighter_->highlightLine("See [[another note]] for details");
    EXPECT_TRUE(result.success);
    
    bool found_wiki_link = false;
    for (const auto& seg : result.segments) {
        if (seg.element_type == "wiki_link") {
            found_wiki_link = true;
            EXPECT_EQ(seg.start_pos, 4);   // Start of "[[another note]]"
            EXPECT_EQ(seg.end_pos, 20);    // End of "[[another note]]"
            break;
        }
    }
    EXPECT_TRUE(found_wiki_link);
}

// Tag Tests

TEST_F(MarkdownHighlighterTest, HighlightTags) {
    auto result = highlighter_->highlightLine("This note has #tag1 and #tag2");
    EXPECT_TRUE(result.success);
    
    int tag_count = 0;
    for (const auto& seg : result.segments) {
        if (seg.element_type == "tag") {
            tag_count++;
        }
    }
    EXPECT_EQ(tag_count, 2);
}

TEST_F(MarkdownHighlighterTest, IgnoreTagsInMiddleOfWords) {
    auto result = highlighter_->highlightLine("This is email@domain.com not a tag");
    EXPECT_TRUE(result.success);
    
    bool found_tag = false;
    for (const auto& seg : result.segments) {
        if (seg.element_type == "tag") {
            found_tag = true;
            break;
        }
    }
    EXPECT_FALSE(found_tag);
}

// Horizontal Rule Tests

TEST_F(MarkdownHighlighterTest, HighlightHorizontalRules) {
    auto result1 = highlighter_->highlightLine("---");
    auto result2 = highlighter_->highlightLine("***");
    auto result3 = highlighter_->highlightLine("___");
    auto result4 = highlighter_->highlightLine("-----");
    
    for (auto& result : {result1, result2, result3, result4}) {
        EXPECT_TRUE(result.success);
        
        bool found_hr = false;
        for (const auto& seg : result.segments) {
            if (seg.element_type == "horizontal_rule") {
                found_hr = true;
                break;
            }
        }
        EXPECT_TRUE(found_hr);
    }
}

TEST_F(MarkdownHighlighterTest, IgnoreInvalidHorizontalRules) {
    auto result1 = highlighter_->highlightLine("--");  // Too short
    auto result2 = highlighter_->highlightLine("-- text");  // Has text
    
    for (auto& result : {result1, result2}) {
        EXPECT_TRUE(result.success);
        
        bool found_hr = false;
        for (const auto& seg : result.segments) {
            if (seg.element_type == "horizontal_rule") {
                found_hr = true;
                break;
            }
        }
        EXPECT_FALSE(found_hr);
    }
}

// Configuration Tests

TEST_F(MarkdownHighlighterTest, DisableHighlighting) {
    config_.enabled = false;
    highlighter_->setConfig(config_);
    
    auto result = highlighter_->highlightLine("# Header with **bold** text");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.segments.size(), 0);
}

TEST_F(MarkdownHighlighterTest, DisableSpecificElements) {
    config_.highlight_headers = false;
    config_.highlight_emphasis = false;
    highlighter_->setConfig(config_);
    
    auto result = highlighter_->highlightLine("# Header with **bold** text");
    EXPECT_TRUE(result.success);
    
    bool found_header = false;
    bool found_bold = false;
    
    for (const auto& seg : result.segments) {
        if (seg.element_type == "header_text") found_header = true;
        if (seg.element_type == "bold") found_bold = true;
    }
    
    EXPECT_FALSE(found_header);
    EXPECT_FALSE(found_bold);
}

// Multi-line Tests

TEST_F(MarkdownHighlighterTest, HighlightMultipleLines) {
    std::vector<std::string> lines = {
        "# Header",
        "This is **bold** text",
        "- List item",
        "> Quote"
    };
    
    auto results = highlighter_->highlightLines(lines);
    EXPECT_EQ(results.size(), 4);
    
    for (const auto& result : results) {
        EXPECT_TRUE(result.success);
        EXPECT_GE(result.segments.size(), 1);
    }
}

TEST_F(MarkdownHighlighterTest, HandleCodeBlocks) {
    std::vector<std::string> lines = {
        "Text before",
        "```",
        "code line 1",
        "code line 2",
        "```",
        "Text after"
    };
    
    auto results = highlighter_->highlightLines(lines);
    EXPECT_EQ(results.size(), 6);
    
    // Lines 2 and 3 (indices 2, 3) should be highlighted as code blocks
    bool found_code_block_line1 = false;
    bool found_code_block_line2 = false;
    
    for (const auto& seg : results[2].segments) {
        if (seg.element_type == "code_block") {
            found_code_block_line1 = true;
            break;
        }
    }
    
    for (const auto& seg : results[3].segments) {
        if (seg.element_type == "code_block") {
            found_code_block_line2 = true;
            break;
        }
    }
    
    EXPECT_TRUE(found_code_block_line1);
    EXPECT_TRUE(found_code_block_line2);
}

// Theme Tests

TEST_F(MarkdownHighlighterTest, ApplyDarkTheme) {
    auto dark_config = HighlightThemes::getDarkTheme();
    highlighter_->setConfig(dark_config);
    
    auto result = highlighter_->highlightLine("# Header");
    EXPECT_TRUE(result.success);
    EXPECT_GE(result.segments.size(), 1);
}

TEST_F(MarkdownHighlighterTest, ApplyMinimalTheme) {
    auto minimal_config = HighlightThemes::getMinimalTheme();
    highlighter_->setConfig(minimal_config);
    
    auto result = highlighter_->highlightLine("**bold** text");
    EXPECT_TRUE(result.success);
    EXPECT_GE(result.segments.size(), 1);
}

// Edge Cases

TEST_F(MarkdownHighlighterTest, HandleEmptyText) {
    auto result = highlighter_->highlightLine("");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.segments.size(), 0);
}

TEST_F(MarkdownHighlighterTest, HandleComplexCombinations) {
    auto result = highlighter_->highlightLine("# Header with **bold** and *italic* and `code` and [link](url) and #tag");
    EXPECT_TRUE(result.success);
    EXPECT_GE(result.segments.size(), 5);  // Should have multiple highlighted segments
}

TEST_F(MarkdownHighlighterTest, HandleNestedEmphasis) {
    auto result = highlighter_->highlightLine("This has ***bold and italic*** text");
    EXPECT_TRUE(result.success);
    
    bool found_bold_italic = false;
    for (const auto& seg : result.segments) {
        if (seg.element_type == "bold_italic") {
            found_bold_italic = true;
            break;
        }
    }
    EXPECT_TRUE(found_bold_italic);
}

// Optimization Tests

TEST_F(MarkdownHighlighterTest, OptimizeOverlappingSegments) {
    HighlightResult result;
    result.addSegment(0, 5, TextStyle{});
    result.addSegment(3, 8, TextStyle{});  // Overlapping with same style
    result.addSegment(10, 15, TextStyle{});
    
    EXPECT_EQ(result.segments.size(), 3);
    result.optimize();
    EXPECT_EQ(result.segments.size(), 2);  // Should merge overlapping segments
}

TEST_F(MarkdownHighlighterTest, GetStyleAtPosition) {
    HighlightResult result;
    TextStyle style1{ftxui::Color::Red};
    TextStyle style2{ftxui::Color::Blue};
    
    result.addSegment(0, 5, style1);
    result.addSegment(10, 15, style2);
    
    EXPECT_EQ(result.getStyleAt(2).foreground, ftxui::Color::Red);
    EXPECT_EQ(result.getStyleAt(7).foreground, ftxui::Color::Default);  // No style
    EXPECT_EQ(result.getStyleAt(12).foreground, ftxui::Color::Blue);
}