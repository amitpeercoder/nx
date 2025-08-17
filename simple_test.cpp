#include <gtest/gtest.h>
#include "nx/tui/editor_buffer.hpp"

using namespace nx::tui;
using namespace nx;

TEST(SimpleTest, BasicInsertChar) {
    EditorBuffer::Config config;
    config.gap_config.initial_gap_size = 64;
    config.gap_config.max_buffer_size = 1024 * 1024;
    EditorBuffer buffer(config);
    
    auto init_result = buffer.initialize("Hello\nWorld\nTest");
    ASSERT_TRUE(init_result.has_value()) << "Initialize failed: " << init_result.error().message();
    
    auto line0_before = buffer.getLine(0);
    ASSERT_TRUE(line0_before.has_value()) << "getLine failed: " << line0_before.error().message();
    EXPECT_EQ(line0_before.value(), "Hello");
    
    // Try inserting ' ' at position (0, 5) - at the end of "Hello"
    auto insert_result = buffer.insertChar(0, 5, ' ');
    EXPECT_TRUE(insert_result.has_value()) << "insertChar failed: " << insert_result.error().message();
    
    auto line0_after = buffer.getLine(0);
    ASSERT_TRUE(line0_after.has_value()) << "getLine failed: " << line0_after.error().message();
    EXPECT_EQ(line0_after.value(), "Hello ");
}