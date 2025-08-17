# Error Handling Guidelines

This document outlines the structured error handling patterns implemented in the nx project.

## Overview

The nx project uses a comprehensive error handling system built on top of C++23's `std::expected` that provides:

- **Contextual errors** with file paths, operations, and call stacks
- **Severity levels** (Info, Warning, Error, Critical)
- **Automatic recovery strategies** for common error scenarios
- **Structured logging** with rotation and multi-sink support
- **Consistent CLI output** with JSON and human-readable formats

## Core Components

### 1. ContextualError (`nx::util::ContextualError`)

Enhanced error type that includes:
- Error code and message
- Optional context (file path, operation, stack trace)
- Severity level
- Recovery information

```cpp
// Creating a contextual error
auto error = util::makeContextualError(
  ErrorCode::kFileNotFound,
  "Configuration file not found",
  NX_FILE_ERROR_CONTEXT("/path/to/config.toml"),
  util::ErrorSeverity::kWarning
);
```

### 2. Error Context Macros

Convenient macros for creating error context:

```cpp
// Basic context with function name
NX_ERROR_CONTEXT()

// File operation context
NX_FILE_ERROR_CONTEXT("/path/to/file")
```

### 3. CommandErrorHandler (`nx::cli::CommandErrorHandler`)

CLI-specific error handler that:
- Formats errors for user display (JSON or human-readable)
- Logs errors appropriately
- Returns appropriate exit codes
- Shows helpful suggestions

```cpp
CommandErrorHandler error_handler(options);
return error_handler.handleCommandError(error);
```

## Error Severity Levels

| Severity | Description | Exit Code | Examples |
|----------|-------------|-----------|----------|
| `kInfo` | Informational messages | 0 | Successful operations with notes |
| `kWarning` | Recoverable issues | 0 | Missing optional files, non-critical failures |
| `kError` | Serious errors | 1 | File not found, permission denied |
| `kCritical` | Critical errors | 2 | Data corruption, system failures |

## Recovery Strategies

The system includes built-in recovery strategies:

### Automatic Recovery
- **File/Directory not found**: Create missing directories
- **Network errors**: Retry with exponential backoff
- **External tool errors**: Try alternative tools

### Manual Recovery
- **User prompts**: Ask user for confirmation or alternative actions
- **Fallback paths**: Use alternative file locations
- **Graceful degradation**: Continue with reduced functionality

```cpp
// Register custom recovery strategy
auto& handler = util::ErrorHandler::instance();
handler.registerRecoveryStrategy(ErrorCode::kFileNotFound, 
  util::recovery::fallbackPath("/alternative/path"));
```

## Implementation Patterns

### 1. Command Implementation

```cpp
Result<int> MyCommand::execute(const GlobalOptions& options) {
  CommandErrorHandler error_handler(options);
  
  try {
    // Setup recovery strategies
    auto& global_handler = util::ErrorHandler::instance();
    global_handler.registerRecoveryStrategy(ErrorCode::kFileNotFound, 
      util::recovery::createMissingDirectory());
    
    // Operation with context
    util::ErrorContext context = NX_ERROR_CONTEXT()
      .withOperation("load_note")
      .withStack({"MyCommand::execute", "load"});
    
    auto result = app_.noteStore().load(note_id);
    if (!result.has_value()) {
      auto ctx_error = util::makeContextualError(
        result.error().code(),
        "Failed to load note: " + result.error().message(),
        context,
        util::ErrorSeverity::kError
      );
      return error_handler.handleCommandError(ctx_error);
    }
    
    // Success
    error_handler.displaySuccess("Operation completed");
    return 0;
    
  } catch (const std::exception& e) {
    auto ctx_error = util::makeContextualError(
      ErrorCode::kUnknownError,
      "Unexpected error: " + std::string(e.what()),
      NX_ERROR_CONTEXT().withOperation("MyCommand::execute"),
      util::ErrorSeverity::kCritical
    );
    return error_handler.handleCommandError(ctx_error);
  }
}
```

### 2. Legacy Error Migration

For gradual migration from legacy `Result<T>` types:

```cpp
// Convert legacy error to contextual error
auto ctx_error = error_handler.convertLegacyError(legacy_error, "operation_name");
return error_handler.handleCommandError(ctx_error);

// Or use helper macros
NX_TRY_COMMAND(error_handler, result, "load_note");
```

### 3. File Operations

```cpp
util::ErrorContext file_context = NX_FILE_ERROR_CONTEXT(file_path)
  .withOperation("write_config")
  .withStack({"Config::save", "writeFileAtomic"});

auto write_result = FileSystem::writeFileAtomic(file_path, content);
if (!write_result.has_value()) {
  auto ctx_error = util::makeContextualError(
    ErrorCode::kFileWriteError,
    "Failed to write configuration file",
    file_context,
    util::ErrorSeverity::kError
  );
  return error_handler.handleCommandError(ctx_error);
}
```

## Error Output Formats

### Human-Readable Format
```
Error: Failed to load note: permission denied
  File: /home/user/notes/important.md
  Operation: load_note
  Suggestion: Check file permissions or run with appropriate privileges
```

### JSON Format
```json
{
  "error": true,
  "code": 24,
  "message": "Failed to load note: permission denied",
  "severity": 2,
  "file": "/home/user/notes/important.md",
  "operation": "load_note"
}
```

## Logging

Errors are automatically logged to:
- **File**: `~/.local/share/nx/logs/error.log` (rotating, 5MB, 3 backups)
- **Console**: Warnings and above (when not in JSON mode)

Log format:
```
[2024-08-15 14:30:45.123] [error] [nx_errors] [1:2] Failed to load note: permission denied
[2024-08-15 14:30:45.124] [debug] [nx_errors]   File: /home/user/notes/important.md
[2024-08-15 14:30:45.124] [debug] [nx_errors]   Operation: load_note
```

## Best Practices

### 1. Use Appropriate Severity Levels
- `kInfo`: Status updates, successful operations
- `kWarning`: Missing optional features, non-critical failures
- `kError`: Operation failures that prevent task completion
- `kCritical`: Data loss risk, system integrity issues

### 2. Provide Context
Always include relevant context:
- File paths for file operations
- Operation names for complex workflows
- Call stacks for debugging

### 3. Implement Recovery
Consider what recovery strategies make sense:
- Can missing directories be created?
- Should network operations be retried?
- Are there alternative tools or paths?

### 4. User-Friendly Messages
- Explain what went wrong
- Suggest concrete solutions
- Avoid technical jargon in user-facing messages

### 5. Consistent Error Codes
Use appropriate error codes from the `ErrorCode` enum:
- `kFileNotFound` for missing files
- `kPermissionDenied` for access issues
- `kValidationError` for input validation
- `kExternalToolError` for third-party tool failures

## Testing Error Handling

### Unit Tests
```cpp
TEST(CommandTest, HandlesFileNotFoundError) {
  // Setup mock to return file not found error
  EXPECT_CALL(mock_store, load(_))
    .WillOnce(Return(std::unexpected(makeError(ErrorCode::kFileNotFound, "File not found"))));
  
  // Execute command
  auto result = command.execute(options);
  
  // Verify error handling
  EXPECT_EQ(result, 1);
  // Additional assertions for error output, logging, etc.
}
```

### Integration Tests
- Test recovery strategies work correctly
- Verify error logging is functioning
- Check JSON vs human-readable output formats
- Ensure appropriate exit codes

## Migration Strategy

1. **Phase 1**: Implement new error handling infrastructure ✅
2. **Phase 2**: Update critical commands (edit, new, remove) ✅
3. **Phase 3**: Migrate remaining commands
4. **Phase 4**: Remove legacy error handling patterns
5. **Phase 5**: Add comprehensive error handling tests

## Error Code Reference

| Code | Name | Description | Recovery Options |
|------|------|-------------|------------------|
| 0 | `kSuccess` | Operation successful | N/A |
| 1 | `kInvalidArgument` | Invalid input parameters | Validation, user prompt |
| 2 | `kFileNotFound` | File does not exist | Create, fallback path |
| 3 | `kFileReadError` | Cannot read file | Permission fix, retry |
| 4 | `kFileWriteError` | Cannot write file | Permission fix, retry |
| 5 | `kFilePermissionDenied` | Access denied | Permission change, sudo |
| ... | ... | ... | ... |

## Troubleshooting

### Common Issues

1. **Errors not being logged**
   - Check log directory permissions
   - Verify `setupErrorHandling()` is called during startup

2. **Recovery strategies not working**
   - Ensure strategies are registered before first use
   - Check strategy implementation returns `std::optional<std::string>`

3. **JSON output malformed**
   - Verify all error paths use `CommandErrorHandler`
   - Check for direct `std::cout` usage in error cases

4. **Performance impact**
   - Error context creation is lightweight
   - Logging is asynchronous where possible
   - Recovery strategies only run on actual errors