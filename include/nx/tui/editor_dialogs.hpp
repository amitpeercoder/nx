#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include "nx/common.hpp"
#include "nx/tui/editor_search.hpp"

namespace nx::tui {

/**
 * @brief Base class for editor dialogs
 */
class EditorDialog {
public:
    virtual ~EditorDialog() = default;
    
    /**
     * @brief Show the dialog and handle user interaction
     * @return Result indicating success or cancellation
     */
    virtual Result<bool> show() = 0;
    
    /**
     * @brief Check if dialog was cancelled
     * @return true if user cancelled, false if completed
     */
    virtual bool wasCancelled() const = 0;

protected:
    bool cancelled_ = false;
};

/**
 * @brief Find dialog for text search
 */
class FindDialog : public EditorDialog {
public:
    struct FindOptions {
        bool case_sensitive = false;
        bool whole_words = false;
        bool regex_mode = false;
        bool wrap_search = true;
    };
    
    struct FindResult {
        std::string query;
        FindOptions options;
        bool find_next = false;  // true for "Find Next", false for "Find All"
    };
    
    explicit FindDialog(const std::string& initial_query = "");
    ~FindDialog() override = default;
    
    Result<bool> show() override;
    bool wasCancelled() const override { return cancelled_; }
    
    const FindResult& getResult() const { return result_; }

private:
    std::string initial_query_;
    FindResult result_;
    
    // FTXUI components
    ftxui::Component container_;
    ftxui::Component input_field_;
    ftxui::Component case_sensitive_checkbox_;
    ftxui::Component whole_words_checkbox_;
    ftxui::Component regex_checkbox_;
    ftxui::Component wrap_checkbox_;
    ftxui::Component find_next_button_;
    ftxui::Component find_all_button_;
    ftxui::Component cancel_button_;
    
    // State
    std::string query_text_;
    bool show_dialog_ = true;
    
    void setupComponents();
    ftxui::Element renderDialog();
    void handleFindNext();
    void handleFindAll();
    void handleCancel();
};

/**
 * @brief Go-to-line dialog
 */
class GotoLineDialog : public EditorDialog {
public:
    explicit GotoLineDialog(size_t current_line = 1, size_t max_line = 1);
    ~GotoLineDialog() override = default;
    
    Result<bool> show() override;
    bool wasCancelled() const override { return cancelled_; }
    
    size_t getTargetLine() const { return target_line_; }

private:
    size_t current_line_;
    size_t max_line_;
    size_t target_line_;
    
    // FTXUI components
    ftxui::Component container_;
    ftxui::Component input_field_;
    ftxui::Component ok_button_;
    ftxui::Component cancel_button_;
    
    // State
    std::string line_text_;
    bool show_dialog_ = true;
    
    void setupComponents();
    ftxui::Element renderDialog();
    void handleOk();
    void handleCancel();
    bool validateLineNumber(const std::string& text, size_t& line_num);
};

/**
 * @brief Replace dialog for find and replace operations
 */
class ReplaceDialog : public EditorDialog {
public:
    struct ReplaceOptions {
        bool case_sensitive = false;
        bool whole_words = false;
        bool regex_mode = false;
        bool wrap_search = true;
    };
    
    struct ReplaceResult {
        std::string find_query;
        std::string replace_text;
        ReplaceOptions options;
        enum Action { Replace, ReplaceAll, Cancel } action = Cancel;
    };
    
    explicit ReplaceDialog(const std::string& initial_query = "");
    ~ReplaceDialog() override = default;
    
    Result<bool> show() override;
    bool wasCancelled() const override { return cancelled_; }
    
    const ReplaceResult& getResult() const { return result_; }

private:
    std::string initial_query_;
    ReplaceResult result_;
    
    // FTXUI components
    ftxui::Component container_;
    ftxui::Component find_input_;
    ftxui::Component replace_input_;
    ftxui::Component case_sensitive_checkbox_;
    ftxui::Component whole_words_checkbox_;
    ftxui::Component regex_checkbox_;
    ftxui::Component wrap_checkbox_;
    ftxui::Component replace_button_;
    ftxui::Component replace_all_button_;
    ftxui::Component cancel_button_;
    
    // State
    std::string find_text_;
    std::string replace_text_;
    bool show_dialog_ = true;
    
    void setupComponents();
    ftxui::Element renderDialog();
    void handleReplace();
    void handleReplaceAll();
    void handleCancel();
};

/**
 * @brief Dialog manager for coordinating multiple dialogs
 */
class DialogManager {
public:
    DialogManager() = default;
    ~DialogManager() = default;
    
    /**
     * @brief Show find dialog
     * @param initial_query Initial search query
     * @return Result with find options or cancellation
     */
    Result<FindDialog::FindResult> showFindDialog(const std::string& initial_query = "");
    
    /**
     * @brief Show go-to-line dialog
     * @param current_line Current line number
     * @param max_line Maximum line number
     * @return Result with target line or cancellation
     */
    Result<size_t> showGotoLineDialog(size_t current_line = 1, size_t max_line = 1);
    
    /**
     * @brief Show replace dialog
     * @param initial_query Initial search query
     * @return Result with replace options or cancellation
     */
    Result<ReplaceDialog::ReplaceResult> showReplaceDialog(const std::string& initial_query = "");
    
    /**
     * @brief Show confirmation dialog
     * @param message Message to display
     * @param title Dialog title
     * @return true if confirmed, false if cancelled
     */
    bool showConfirmationDialog(const std::string& message, const std::string& title = "Confirm");
    
    /**
     * @brief Show error dialog
     * @param message Error message to display
     * @param title Dialog title
     */
    void showErrorDialog(const std::string& message, const std::string& title = "Error");
    
    /**
     * @brief Show information dialog
     * @param message Information message to display
     * @param title Dialog title
     */
    void showInfoDialog(const std::string& message, const std::string& title = "Information");

private:
    ftxui::ScreenInteractive screen_ = ftxui::ScreenInteractive::Fullscreen();
    
    // Helper methods
    ftxui::Element createDialogBox(const ftxui::Element& content, const std::string& title);
    ftxui::Component createButton(const std::string& text, std::function<void()> callback);
};

/**
 * @brief Search results display widget
 */
class SearchResultsWidget {
public:
    explicit SearchResultsWidget(const SearchState* search_state);
    ~SearchResultsWidget() = default;
    
    /**
     * @brief Render search results as FTXUI element
     * @param height Maximum height for results display
     * @return FTXUI element showing search results
     */
    ftxui::Element render(int height = 10);
    
    /**
     * @brief Update displayed results
     * @param search_state Updated search state
     */
    void updateResults(const SearchState* search_state);
    
    /**
     * @brief Set current selection
     * @param index Index of currently selected result
     */
    void setCurrentSelection(int index);

private:
    const SearchState* search_state_;
    int current_selection_ = -1;
    
    ftxui::Element renderResultLine(const SearchMatch& match, bool is_current);
    std::string formatResultSummary();
};

} // namespace nx::tui