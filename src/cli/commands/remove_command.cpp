#include "nx/cli/commands/remove_command.hpp"

#include <iostream>
#include <nlohmann/json.hpp>
#include "nx/cli/command_error_handler.hpp"
#include "nx/util/error_handler.hpp"

namespace nx::cli {

RemoveCommand::RemoveCommand(Application& app) : app_(app) {
}

Result<int> RemoveCommand::execute(const GlobalOptions& options) {
  CommandErrorHandler error_handler(options);
  
  try {
    // Setup recovery strategies
    auto& global_handler = util::ErrorHandler::instance();
    global_handler.registerRecoveryStrategy(ErrorCode::kNotFound, 
      util::recovery::promptUser("Note not found. Continue anyway?"));
    
    // Resolve the note ID with enhanced error context
    util::ErrorContext resolve_context = NX_ERROR_CONTEXT()
      .withOperation("resolve_note_id")
      .withStack({"RemoveCommand::execute", "resolveSingle"});
    
    auto resolved_id = app_.noteStore().resolveSingle(note_id_);
    if (!resolved_id.has_value()) {
      auto ctx_error = util::makeContextualError(
        resolved_id.error().code(),
        "Failed to resolve note ID '" + note_id_ + "': " + resolved_id.error().message(),
        resolve_context,
        util::ErrorSeverity::kError
      );
      return error_handler.handleCommandError(ctx_error);
    }

    // Check if note exists with context
    util::ErrorContext exists_context = NX_ERROR_CONTEXT()
      .withOperation("check_note_exists")
      .withStack({"RemoveCommand::execute", "exists"});
    
    auto exists = app_.noteStore().exists(*resolved_id);
    if (!exists.has_value()) {
      auto ctx_error = util::makeContextualError(
        exists.error().code(),
        "Failed to check if note exists: " + exists.error().message(),
        exists_context,
        util::ErrorSeverity::kError
      );
      return error_handler.handleCommandError(ctx_error);
    }
    
    if (!*exists) {
      auto ctx_error = util::makeContextualError(
        ErrorCode::kNotFound,
        "Note not found: " + resolved_id->toString(),
        exists_context,
        util::ErrorSeverity::kWarning
      );
      return error_handler.handleCommandError(ctx_error);
    }

    // Remove the note with enhanced error handling
    util::ErrorContext remove_context = NX_ERROR_CONTEXT()
      .withOperation(permanent_ ? "permanent_delete" : "soft_delete")
      .withStack({"RemoveCommand::execute", "remove"});
    
    if (permanent_ && !options.force) {
      error_handler.displayWarning("Permanent deletion requested. This cannot be undone.");
      // In a real implementation, we might want to prompt for confirmation
    }
    
    auto remove_result = app_.noteStore().remove(*resolved_id, !permanent_);
    if (!remove_result.has_value()) {
      auto ctx_error = util::makeContextualError(
        remove_result.error().code(),
        "Failed to remove note: " + remove_result.error().message(),
        remove_context,
        permanent_ ? util::ErrorSeverity::kCritical : util::ErrorSeverity::kError
      );
      return error_handler.handleCommandError(ctx_error);
    }

    // Update search index with graceful degradation
    util::ErrorContext index_context = NX_ERROR_CONTEXT()
      .withOperation("update_search_index")
      .withStack({"RemoveCommand::execute", "removeNote"});
    
    auto index_result = app_.searchIndex().removeNote(*resolved_id);
    if (!index_result.has_value()) {
      auto ctx_error = util::makeContextualError(
        index_result.error().code(),
        "Failed to update search index: " + index_result.error().message(),
        index_context,
        util::ErrorSeverity::kWarning  // Non-critical, note was still removed
      );
      error_handler.displayWarning("Search index update failed, but note was removed successfully");
    }

    // Output success result
    if (options.json) {
      nlohmann::json result_json;
      result_json["success"] = true;
      result_json["note_id"] = resolved_id->toString();
      result_json["permanent"] = permanent_;
      result_json["operation"] = permanent_ ? "permanent_delete" : "soft_delete";
      std::cout << result_json.dump() << std::endl;
    } else {
      if (permanent_) {
        error_handler.displaySuccess("Permanently deleted note: " + resolved_id->toString());
      } else {
        error_handler.displaySuccess("Moved note to trash: " + resolved_id->toString());
      }
    }

    return 0;

  } catch (const std::exception& e) {
    auto ctx_error = util::makeContextualError(
      ErrorCode::kUnknownError,
      "Unexpected error during note removal: " + std::string(e.what()),
      NX_ERROR_CONTEXT().withOperation("RemoveCommand::execute"),
      util::ErrorSeverity::kCritical
    );
    return error_handler.handleCommandError(ctx_error);
  }
}

void RemoveCommand::setupCommand(CLI::App* cmd) {
  cmd->add_option("note_id", note_id_, "Note ID to remove (can be partial)")->required();
  cmd->add_flag("--permanent", permanent_, "Permanently delete (skip trash)");
}

} // namespace nx::cli