#include "nx/tui/editor_dialogs.hpp"
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>
#include <sstream>
#include <algorithm>

using namespace ftxui;

namespace nx::tui {

// FindDialog implementation

FindDialog::FindDialog(const std::string& initial_query) 
    : initial_query_(initial_query), query_text_(initial_query) {
    setupComponents();
}

Result<bool> FindDialog::show() {
    auto screen = ScreenInteractive::Fullscreen();
    
    auto main_container = Container::Vertical({
        container_
    });
    
    auto renderer = Renderer(main_container, [this] {
        return renderDialog();
    });
    
    screen.Loop(renderer);
    
    return !cancelled_;
}

void FindDialog::setupComponents() {
    // Input field for search query
    input_field_ = Input(&query_text_, "Enter search query...");
    
    // Checkboxes for options
    case_sensitive_checkbox_ = Checkbox("Case sensitive", &result_.options.case_sensitive);
    whole_words_checkbox_ = Checkbox("Whole words", &result_.options.whole_words);
    regex_checkbox_ = Checkbox("Regular expression", &result_.options.regex_mode);
    wrap_checkbox_ = Checkbox("Wrap search", &result_.options.wrap_search);
    
    // Buttons
    find_next_button_ = Button("Find Next", [this] { handleFindNext(); });
    find_all_button_ = Button("Find All", [this] { handleFindAll(); });
    cancel_button_ = Button("Cancel", [this] { handleCancel(); });
    
    // Main container
    container_ = Container::Vertical({
        input_field_,
        Container::Horizontal({
            case_sensitive_checkbox_,
            whole_words_checkbox_
        }),
        Container::Horizontal({
            regex_checkbox_,
            wrap_checkbox_
        }),
        Container::Horizontal({
            find_next_button_,
            find_all_button_,
            cancel_button_
        })
    });
    
    // Set initial focus to input field
    container_->SetActiveChild(input_field_);
}

Element FindDialog::renderDialog() {
    auto content = vbox({
        text("Find") | bold | center,
        separator(),
        hbox({text("Search for: "), input_field_->Render()}),
        separator(),
        hbox({
            case_sensitive_checkbox_->Render(),
            text("  "),
            whole_words_checkbox_->Render()
        }),
        hbox({
            regex_checkbox_->Render(),
            text("  "),
            wrap_checkbox_->Render()
        }),
        separator(),
        hbox({
            find_next_button_->Render(),
            text("  "),
            find_all_button_->Render(),
            text("  "),
            cancel_button_->Render()
        })
    });
    
    return window(text("Find"), content) | size(WIDTH, GREATER_THAN, 50) | center;
}

void FindDialog::handleFindNext() {
    result_.query = query_text_;
    result_.find_next = true;
    cancelled_ = false;
    show_dialog_ = false;
}

void FindDialog::handleFindAll() {
    result_.query = query_text_;
    result_.find_next = false;
    cancelled_ = false;
    show_dialog_ = false;
}

void FindDialog::handleCancel() {
    cancelled_ = true;
    show_dialog_ = false;
}

// GotoLineDialog implementation

GotoLineDialog::GotoLineDialog(size_t current_line, size_t max_line)
    : current_line_(current_line), max_line_(max_line), target_line_(current_line) {
    line_text_ = std::to_string(current_line);
    setupComponents();
}

Result<bool> GotoLineDialog::show() {
    auto screen = ScreenInteractive::Fullscreen();
    
    auto main_container = Container::Vertical({
        container_
    });
    
    auto renderer = Renderer(main_container, [this] {
        return renderDialog();
    });
    
    screen.Loop(renderer);
    
    return !cancelled_;
}

void GotoLineDialog::setupComponents() {
    // Input field for line number
    input_field_ = Input(&line_text_, "Line number...");
    
    // Buttons
    ok_button_ = Button("Go", [this] { handleOk(); });
    cancel_button_ = Button("Cancel", [this] { handleCancel(); });
    
    // Main container
    container_ = Container::Vertical({
        input_field_,
        Container::Horizontal({
            ok_button_,
            cancel_button_
        })
    });
    
    // Set initial focus to input field
    container_->SetActiveChild(input_field_);
}

Element GotoLineDialog::renderDialog() {
    auto content = vbox({
        text("Go to Line") | bold | center,
        separator(),
        hbox({
            text("Line (1-" + std::to_string(max_line_) + "): "),
            input_field_->Render()
        }),
        text("Current line: " + std::to_string(current_line_)) | dim,
        separator(),
        hbox({
            ok_button_->Render(),
            text("  "),
            cancel_button_->Render()
        })
    });
    
    return window(text("Go to Line"), content) | size(WIDTH, GREATER_THAN, 40) | center;
}

void GotoLineDialog::handleOk() {
    size_t line_num;
    if (validateLineNumber(line_text_, line_num)) {
        target_line_ = line_num;
        cancelled_ = false;
        show_dialog_ = false;
    }
}

void GotoLineDialog::handleCancel() {
    cancelled_ = true;
    show_dialog_ = false;
}

bool GotoLineDialog::validateLineNumber(const std::string& text, size_t& line_num) {
    try {
        line_num = std::stoull(text);
        return line_num >= 1 && line_num <= max_line_;
    } catch (...) {
        return false;
    }
}

// ReplaceDialog implementation

ReplaceDialog::ReplaceDialog(const std::string& initial_query) 
    : initial_query_(initial_query), find_text_(initial_query) {
    setupComponents();
}

Result<bool> ReplaceDialog::show() {
    auto screen = ScreenInteractive::Fullscreen();
    
    auto main_container = Container::Vertical({
        container_
    });
    
    auto renderer = Renderer(main_container, [this] {
        return renderDialog();
    });
    
    screen.Loop(renderer);
    
    return !cancelled_;
}

void ReplaceDialog::setupComponents() {
    // Input fields
    find_input_ = Input(&find_text_, "Find...");
    replace_input_ = Input(&replace_text_, "Replace with...");
    
    // Checkboxes for options
    case_sensitive_checkbox_ = Checkbox("Case sensitive", &result_.options.case_sensitive);
    whole_words_checkbox_ = Checkbox("Whole words", &result_.options.whole_words);
    regex_checkbox_ = Checkbox("Regular expression", &result_.options.regex_mode);
    wrap_checkbox_ = Checkbox("Wrap search", &result_.options.wrap_search);
    
    // Buttons
    replace_button_ = Button("Replace", [this] { handleReplace(); });
    replace_all_button_ = Button("Replace All", [this] { handleReplaceAll(); });
    cancel_button_ = Button("Cancel", [this] { handleCancel(); });
    
    // Main container
    container_ = Container::Vertical({
        find_input_,
        replace_input_,
        Container::Horizontal({
            case_sensitive_checkbox_,
            whole_words_checkbox_
        }),
        Container::Horizontal({
            regex_checkbox_,
            wrap_checkbox_
        }),
        Container::Horizontal({
            replace_button_,
            replace_all_button_,
            cancel_button_
        })
    });
    
    // Set initial focus to find input
    container_->SetActiveChild(find_input_);
}

Element ReplaceDialog::renderDialog() {
    auto content = vbox({
        text("Find and Replace") | bold | center,
        separator(),
        hbox({text("Find:    "), find_input_->Render()}),
        hbox({text("Replace: "), replace_input_->Render()}),
        separator(),
        hbox({
            case_sensitive_checkbox_->Render(),
            text("  "),
            whole_words_checkbox_->Render()
        }),
        hbox({
            regex_checkbox_->Render(),
            text("  "),
            wrap_checkbox_->Render()
        }),
        separator(),
        hbox({
            replace_button_->Render(),
            text("  "),
            replace_all_button_->Render(),
            text("  "),
            cancel_button_->Render()
        })
    });
    
    return window(text("Replace"), content) | size(WIDTH, GREATER_THAN, 60) | center;
}

void ReplaceDialog::handleReplace() {
    result_.find_query = find_text_;
    result_.replace_text = replace_text_;
    result_.action = ReplaceResult::Replace;
    cancelled_ = false;
    show_dialog_ = false;
}

void ReplaceDialog::handleReplaceAll() {
    result_.find_query = find_text_;
    result_.replace_text = replace_text_;
    result_.action = ReplaceResult::ReplaceAll;
    cancelled_ = false;
    show_dialog_ = false;
}

void ReplaceDialog::handleCancel() {
    cancelled_ = true;
    show_dialog_ = false;
}

// DialogManager implementation

Result<FindDialog::FindResult> DialogManager::showFindDialog(const std::string& initial_query) {
    FindDialog dialog(initial_query);
    auto result = dialog.show();
    
    if (!result || dialog.wasCancelled()) {
        return makeErrorResult<FindDialog::FindResult>(ErrorCode::kInvalidArgument, "Find dialog cancelled");
    }
    
    return dialog.getResult();
}

Result<size_t> DialogManager::showGotoLineDialog(size_t current_line, size_t max_line) {
    GotoLineDialog dialog(current_line, max_line);
    auto result = dialog.show();
    
    if (!result || dialog.wasCancelled()) {
        return makeErrorResult<size_t>(ErrorCode::kInvalidArgument, "Go to line dialog cancelled");
    }
    
    return dialog.getTargetLine();
}

Result<ReplaceDialog::ReplaceResult> DialogManager::showReplaceDialog(const std::string& initial_query) {
    ReplaceDialog dialog(initial_query);
    auto result = dialog.show();
    
    if (!result || dialog.wasCancelled()) {
        return makeErrorResult<ReplaceDialog::ReplaceResult>(ErrorCode::kInvalidArgument, "Replace dialog cancelled");
    }
    
    return dialog.getResult();
}

bool DialogManager::showConfirmationDialog(const std::string& message, const std::string& title) {
    bool confirmed = false;
    bool show_dialog = true;
    
    auto ok_button = Button("OK", [&] { confirmed = true; show_dialog = false; });
    auto cancel_button = Button("Cancel", [&] { confirmed = false; show_dialog = false; });
    
    auto container = Container::Horizontal({ok_button, cancel_button});
    
    auto renderer = Renderer(container, [&] {
        auto content = vbox({
            text(title) | bold | center,
            separator(),
            paragraph(message),
            separator(),
            hbox({
                ok_button->Render(),
                text("  "),
                cancel_button->Render()
            }) | center
        });
        
        return window(text(title), content) | size(WIDTH, GREATER_THAN, 40) | center;
    });
    
    screen_.Loop(renderer);
    return confirmed;
}

void DialogManager::showErrorDialog(const std::string& message, const std::string& title) {
    bool show_dialog = true;
    
    auto ok_button = Button("OK", [&] { show_dialog = false; });
    
    auto renderer = Renderer(ok_button, [&] {
        auto content = vbox({
            text(title) | bold | center | color(Color::Red),
            separator(),
            paragraph(message),
            separator(),
            ok_button->Render() | center
        });
        
        return window(text(title), content) | size(WIDTH, GREATER_THAN, 40) | center;
    });
    
    screen_.Loop(renderer);
}

void DialogManager::showInfoDialog(const std::string& message, const std::string& title) {
    bool show_dialog = true;
    
    auto ok_button = Button("OK", [&] { show_dialog = false; });
    
    auto renderer = Renderer(ok_button, [&] {
        auto content = vbox({
            text(title) | bold | center | color(Color::Blue),
            separator(),
            paragraph(message),
            separator(),
            ok_button->Render() | center
        });
        
        return window(text(title), content) | size(WIDTH, GREATER_THAN, 40) | center;
    });
    
    screen_.Loop(renderer);
}

Element DialogManager::createDialogBox(const Element& content, const std::string& title) {
    return window(text(title), content) | center;
}

Component DialogManager::createButton(const std::string& text, std::function<void()> callback) {
    return Button(text, callback);
}

// SearchResultsWidget implementation

SearchResultsWidget::SearchResultsWidget(const SearchState* search_state)
    : search_state_(search_state) {
}

Element SearchResultsWidget::render(int height) {
    if (!search_state_ || !search_state_->hasResults()) {
        return vbox({
            text("No search results") | dim | center
        }) | size(HEIGHT, EQUAL, height);
    }
    
    Elements result_elements;
    
    // Add summary header
    result_elements.push_back(text(formatResultSummary()) | bold);
    result_elements.push_back(separator());
    
    // Add individual results
    const auto& results = search_state_->getResults();
    int displayed_results = std::min(static_cast<int>(results.size()), height - 3);
    
    for (int i = 0; i < displayed_results; ++i) {
        bool is_current = (i == current_selection_);
        result_elements.push_back(renderResultLine(results[i], is_current));
    }
    
    if (static_cast<int>(results.size()) > displayed_results) {
        result_elements.push_back(text("... and " + 
            std::to_string(results.size() - displayed_results) + " more") | dim);
    }
    
    return vbox(result_elements) | size(HEIGHT, EQUAL, height);
}

void SearchResultsWidget::updateResults(const SearchState* search_state) {
    search_state_ = search_state;
    current_selection_ = -1;
}

void SearchResultsWidget::setCurrentSelection(int index) {
    current_selection_ = index;
}

Element SearchResultsWidget::renderResultLine(const SearchMatch& match, bool is_current) {
    std::string line_info = "Line " + std::to_string(match.line + 1) + ": ";
    
    std::string context = match.context_before + match.matched_text + match.context_after;
    if (context.length() > 80) {
        context = context.substr(0, 77) + "...";
    }
    
    auto line_element = hbox({
        text(line_info) | color(Color::Blue),
        text(context)
    });
    
    if (is_current) {
        line_element = line_element | inverted;
    }
    
    return line_element;
}

std::string SearchResultsWidget::formatResultSummary() {
    if (!search_state_ || !search_state_->hasResults()) {
        return "No results";
    }
    
    const auto& results = search_state_->getResults();
    std::ostringstream oss;
    
    oss << results.size() << " match";
    if (results.size() != 1) {
        oss << "es";
    }
    
    oss << " for \"" << search_state_->getLastQuery() << "\"";
    
    if (search_state_->getLastSearchDuration().count() > 0) {
        oss << " (" << search_state_->getLastSearchDuration().count() << "ms)";
    }
    
    return oss.str();
}

} // namespace nx::tui