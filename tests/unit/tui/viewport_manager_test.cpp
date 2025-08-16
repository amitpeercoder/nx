#include <gtest/gtest.h>
#include "nx/tui/viewport_manager.hpp"

using namespace nx::tui;

class ViewportManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        ViewportConfig config;
        config.scroll_mode = ScrollMode::SmartCenter;
        config.top_margin = 2;
        config.bottom_margin = 2;
        config.left_margin = 3;
        config.right_margin = 5;
        
        viewport_manager_ = std::make_unique<ViewportManager>(config);
        
        // Set up a test document
        viewport_manager_->setDocumentSize(100, 80); // 100 lines, 80 chars wide
        viewport_manager_->setViewportSize(20, 40);   // 20 lines, 40 chars visible
    }
    
    std::unique_ptr<ViewportManager> viewport_manager_;
};

// Basic viewport functionality tests

TEST_F(ViewportManagerTest, InitialState) {
    const auto& viewport = viewport_manager_->getViewport();
    
    EXPECT_EQ(viewport.start_line, 0);
    EXPECT_EQ(viewport.start_column, 0);
    EXPECT_EQ(viewport.visible_lines, 20);
    EXPECT_EQ(viewport.visible_columns, 40);
    EXPECT_EQ(viewport.end_line, 20);
    EXPECT_EQ(viewport.end_column, 40);
}

TEST_F(ViewportManagerTest, SetCursorPosition) {
    auto result = viewport_manager_->setCursorPosition(10, 15);
    ASSERT_TRUE(result.has_value());
    
    const auto& viewport = viewport_manager_->getViewport();
    EXPECT_EQ(viewport.cursor_line, 10);
    EXPECT_EQ(viewport.cursor_column, 15);
    EXPECT_TRUE(viewport_manager_->isCursorVisible());
}

TEST_F(ViewportManagerTest, ScrollToLine) {
    auto result = viewport_manager_->scrollToLine(50);
    ASSERT_TRUE(result.has_value());
    
    const auto& viewport = viewport_manager_->getViewport();
    // With SmartCenter mode, line 50 should be centered
    EXPECT_EQ(viewport.start_line, 40); // 50 - 20/2 = 40
}

TEST_F(ViewportManagerTest, ScrollByLines) {
    auto result = viewport_manager_->scrollBy(10, 5);
    ASSERT_TRUE(result.has_value());
    
    const auto& viewport = viewport_manager_->getViewport();
    EXPECT_EQ(viewport.start_line, 10);
    EXPECT_EQ(viewport.start_column, 5);
}

TEST_F(ViewportManagerTest, ScrollByPages) {
    auto result = viewport_manager_->scrollByPages(2);
    ASSERT_TRUE(result.has_value());
    
    const auto& viewport = viewport_manager_->getViewport();
    // Page size should be visible_lines - 1 = 19
    EXPECT_EQ(viewport.start_line, 38); // 2 * 19 = 38
}

TEST_F(ViewportManagerTest, CenterCursor) {
    viewport_manager_->setCursorPosition(50, 25);
    auto result = viewport_manager_->centerCursor();
    ASSERT_TRUE(result.has_value());
    
    const auto& viewport = viewport_manager_->getViewport();
    EXPECT_EQ(viewport.start_line, 40); // 50 - 20/2 = 40
    EXPECT_EQ(viewport.start_column, 5); // 25 - 40/2 = 5
}

// Boundary tests

TEST_F(ViewportManagerTest, ScrollToTop) {
    viewport_manager_->scrollToLine(50);
    auto result = viewport_manager_->scrollToTop();
    ASSERT_TRUE(result.has_value());
    
    const auto& viewport = viewport_manager_->getViewport();
    EXPECT_EQ(viewport.start_line, 0);
    EXPECT_EQ(viewport.start_column, 0);
}

TEST_F(ViewportManagerTest, ScrollToBottom) {
    auto result = viewport_manager_->scrollToBottom();
    ASSERT_TRUE(result.has_value());
    
    const auto& viewport = viewport_manager_->getViewport();
    EXPECT_EQ(viewport.start_line, 80); // 100 - 20 = 80
}

TEST_F(ViewportManagerTest, ClampToBounds) {
    // Try to scroll beyond document bounds
    auto result = viewport_manager_->scrollToLine(150); // Beyond 100 lines
    ASSERT_TRUE(result.has_value());
    
    const auto& viewport = viewport_manager_->getViewport();
    EXPECT_LE(viewport.start_line, 80); // Should be clamped to max valid position
}

// Margin and visibility tests

TEST_F(ViewportManagerTest, EnsureCursorVisible) {
    // Place cursor near edge where margins would require scrolling
    viewport_manager_->setCursorPosition(1, 1); // Near top-left
    auto result = viewport_manager_->ensureCursorVisible();
    ASSERT_TRUE(result.has_value());
    
    EXPECT_TRUE(viewport_manager_->isCursorVisible());
}

TEST_F(ViewportManagerTest, IsLineVisible) {
    viewport_manager_->scrollToLine(30);
    
    EXPECT_TRUE(viewport_manager_->isLineVisible(35)); // Within viewport
    EXPECT_FALSE(viewport_manager_->isLineVisible(10)); // Before viewport
    EXPECT_FALSE(viewport_manager_->isLineVisible(55)); // After viewport
}

TEST_F(ViewportManagerTest, IsPositionVisible) {
    viewport_manager_->scrollToPosition(30, 30);
    
    EXPECT_TRUE(viewport_manager_->isPositionVisible(35, 25)); // Within viewport
    EXPECT_FALSE(viewport_manager_->isPositionVisible(35, 5)); // Column before viewport
    EXPECT_FALSE(viewport_manager_->isPositionVisible(35, 55)); // Column after viewport
}

// Virtual scrolling tests

TEST_F(ViewportManagerTest, VirtualScrollingLargeFile) {
    // Create a large file that should trigger virtual scrolling
    viewport_manager_->setDocumentSize(20000, 120); // 20k lines
    
    auto result = viewport_manager_->enableVirtualScrolling(true);
    ASSERT_TRUE(result.has_value());
    
    EXPECT_TRUE(viewport_manager_->isVirtualScrollingActive());
    
    const auto& viewport = viewport_manager_->getViewport();
    EXPECT_TRUE(viewport.is_virtual);
    EXPECT_GT(viewport.virtual_end, viewport.virtual_start);
}

TEST_F(ViewportManagerTest, VirtualScrollingSmallFile) {
    // Small file should not allow virtual scrolling
    viewport_manager_->setDocumentSize(50, 80); // Small file
    
    auto result = viewport_manager_->enableVirtualScrolling(true);
    EXPECT_FALSE(result.has_value()); // Should fail for small files
}

// Configuration tests

TEST_F(ViewportManagerTest, UpdateConfig) {
    ViewportConfig new_config;
    new_config.scroll_mode = ScrollMode::Jump;
    new_config.top_margin = 5;
    new_config.bottom_margin = 5;
    
    viewport_manager_->updateConfig(new_config);
    
    const auto& config = viewport_manager_->getConfig();
    EXPECT_EQ(config.scroll_mode, ScrollMode::Jump);
    EXPECT_EQ(config.top_margin, 5);
    EXPECT_EQ(config.bottom_margin, 5);
}

// Performance and statistics tests

TEST_F(ViewportManagerTest, PerformanceStatistics) {
    // Perform several scroll operations
    viewport_manager_->scrollToLine(10);
    viewport_manager_->scrollToLine(20);
    viewport_manager_->scrollBy(5, 0);
    
    auto stats = viewport_manager_->getStatistics();
    EXPECT_EQ(stats.scroll_operations, 3);
    EXPECT_GE(stats.avg_scroll_time.count(), 0); // Allow zero for very fast operations
}

TEST_F(ViewportManagerTest, ResetStatistics) {
    viewport_manager_->scrollToLine(10);
    viewport_manager_->resetStatistics();
    
    auto stats = viewport_manager_->getStatistics();
    EXPECT_EQ(stats.scroll_operations, 0);
    EXPECT_EQ(stats.avg_scroll_time.count(), 0);
}

// Factory tests

TEST_F(ViewportManagerTest, FactoryCreateForEditor) {
    auto editor_viewport = ViewportManagerFactory::createForEditor();
    ASSERT_NE(editor_viewport, nullptr);
    
    const auto& config = editor_viewport->getConfig();
    EXPECT_EQ(config.scroll_mode, ScrollMode::SmartCenter);
    EXPECT_EQ(config.top_margin, 3);
    EXPECT_EQ(config.bottom_margin, 3);
}

TEST_F(ViewportManagerTest, FactoryCreateForPreview) {
    auto preview_viewport = ViewportManagerFactory::createForPreview();
    ASSERT_NE(preview_viewport, nullptr);
    
    const auto& config = preview_viewport->getConfig();
    EXPECT_EQ(config.scroll_mode, ScrollMode::Jump);
    EXPECT_LT(config.top_margin, 3); // Should have smaller margins than editor
}

TEST_F(ViewportManagerTest, FactoryCreateForLargeFiles) {
    auto large_file_viewport = ViewportManagerFactory::createForLargeFiles();
    ASSERT_NE(large_file_viewport, nullptr);
    
    const auto& config = large_file_viewport->getConfig();
    EXPECT_EQ(config.scroll_mode, ScrollMode::Minimal);
    EXPECT_GT(config.virtual_page_size, 100); // Should have larger virtual pages
}

TEST_F(ViewportManagerTest, FactoryCreateMinimal) {
    auto minimal_viewport = ViewportManagerFactory::createMinimal();
    ASSERT_NE(minimal_viewport, nullptr);
    
    const auto& config = minimal_viewport->getConfig();
    EXPECT_EQ(config.scroll_mode, ScrollMode::Jump);
    EXPECT_EQ(config.top_margin, 0);
    EXPECT_EQ(config.bottom_margin, 0);
    EXPECT_FALSE(config.enable_virtual_scrolling);
}

// Edge case tests

TEST_F(ViewportManagerTest, ZeroSizeViewport) {
    auto result = viewport_manager_->setViewportSize(0, 40);
    EXPECT_FALSE(result.has_value()); // Should fail
    
    result = viewport_manager_->setViewportSize(20, 0);
    EXPECT_FALSE(result.has_value()); // Should fail
}

TEST_F(ViewportManagerTest, EmptyDocument) {
    viewport_manager_->setDocumentSize(0, 0);
    
    auto result = viewport_manager_->setCursorPosition(0, 0);
    ASSERT_TRUE(result.has_value());
    
    const auto& viewport = viewport_manager_->getViewport();
    EXPECT_EQ(viewport.cursor_line, 0);
    EXPECT_EQ(viewport.cursor_column, 0);
}

TEST_F(ViewportManagerTest, CursorBeyondDocument) {
    // Try to set cursor beyond document bounds
    auto result = viewport_manager_->setCursorPosition(150, 100); // Beyond bounds
    ASSERT_TRUE(result.has_value());
    
    const auto& viewport = viewport_manager_->getViewport();
    EXPECT_LT(viewport.cursor_line, 100); // Should be clamped to document size
    EXPECT_LE(viewport.cursor_column, 80); // Should be clamped to max line length
}