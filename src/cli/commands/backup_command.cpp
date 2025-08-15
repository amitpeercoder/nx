#include "nx/cli/commands/backup_command.hpp"
#include "nx/cli/application.hpp"
#include "nx/util/filesystem.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <ctime>
#include <nlohmann/json.hpp>

namespace nx::cli {
namespace fs = std::filesystem;

BackupCommand::BackupCommand(Application& app) 
    : app_(app) {
}

void BackupCommand::setupCommand(CLI::App* cmd) {
    cmd->description("Create and manage backups of notes and data");
    
    // Subcommands
    auto create_cmd = cmd->add_subcommand("create", "Create a new backup");
    create_cmd->add_option("backup_path", backup_path_, "Path for the backup file");
    create_cmd->add_flag("--include-attachments", include_attachments_, "Include attachment files");
    create_cmd->add_flag("--include-index", include_index_, "Include search index");
    create_cmd->add_flag("--include-config", include_config_, "Include configuration");
    create_cmd->add_option("--compression", compression_, "Compression format (gzip, none)");
    create_cmd->callback([this]() { subcommand_ = "create"; });
    
    auto list_cmd = cmd->add_subcommand("list", "List available backups");
    list_cmd->callback([this]() { subcommand_ = "list"; });
    
    auto restore_cmd = cmd->add_subcommand("restore", "Restore from backup");
    restore_cmd->add_option("backup_path", backup_path_, "Path to backup file")->required();
    restore_cmd->add_option("--target", restore_path_, "Target directory for restore");
    restore_cmd->add_flag("--force", force_, "Force restore without confirmation");
    restore_cmd->callback([this]() { subcommand_ = "restore"; });
    
    auto verify_cmd = cmd->add_subcommand("verify", "Verify backup integrity");
    verify_cmd->add_option("backup_path", backup_path_, "Path to backup file")->required();
    verify_cmd->callback([this]() { subcommand_ = "verify"; });
    
    auto cleanup_cmd = cmd->add_subcommand("cleanup", "Remove old backups");
    cleanup_cmd->add_flag("--force", force_, "Force cleanup without confirmation");
    cleanup_cmd->callback([this]() { subcommand_ = "cleanup"; });
}

Result<int> BackupCommand::execute(const GlobalOptions& options) {
    if (subcommand_ == "create") {
        return executeCreate();
    } else if (subcommand_ == "list") {
        return executeList();
    } else if (subcommand_ == "restore") {
        return executeRestore();
    } else if (subcommand_ == "verify") {
        return executeVerify();
    } else if (subcommand_ == "cleanup") {
        return executeCleanup();
    }
    
    return std::unexpected(makeError(ErrorCode::kInvalidArgument, "No subcommand specified"));
}

Result<int> BackupCommand::executeCreate() {
    const auto& options = app_.globalOptions();
    
    // Generate backup path if not provided
    fs::path target_path;
    if (backup_path_.empty()) {
        auto backup_name_result = generateBackupName();
        if (!backup_name_result.has_value()) {
            return std::unexpected(makeError(ErrorCode::kFileError, backup_name_result.error().message()));
        }
        
        auto data_dir = app_.config().data_dir;
        auto backup_dir = data_dir / "backups";
        
        std::error_code ec;
        if (!fs::create_directories(backup_dir, ec) && ec) {
            return std::unexpected(makeError(ErrorCode::kDirectoryCreateError, "Failed to create backup directory: " + ec.message()));
        }
        
        target_path = backup_dir / (*backup_name_result + ".tar.gz");
    } else {
        target_path = fs::path(backup_path_);
    }
    
    outputProgress("Creating backup: " + target_path.filename().string(), options);
    
    auto backup_result = createBackup(target_path);
    if (!backup_result.has_value()) {
        return std::unexpected(makeError(ErrorCode::kFileError, backup_result.error().message()));
    }
    
    const auto& backup_info = *backup_result;
    
    if (options.json) {
        nlohmann::json result;
        result["backup_path"] = backup_info.path.string();
        result["size_bytes"] = backup_info.size_bytes;
        result["note_count"] = backup_info.note_count;
        result["has_attachments"] = backup_info.has_attachments;
        result["has_index"] = backup_info.has_index;
        result["has_config"] = backup_info.has_config;
        result["compression"] = backup_info.compression;
        std::cout << result.dump(2) << std::endl;
    } else {
        std::cout << "Backup created successfully\n";
        outputBackupInfo(backup_info, options);
    }
    
    return 0;
}

Result<int> BackupCommand::executeList() {
    const auto& options = app_.globalOptions();
    auto data_dir = app_.config().data_dir;
    auto backup_dir = data_dir / "backups";
    
    if (!fs::exists(backup_dir)) {
        if (options.json) {
            nlohmann::json result;
            result["backups"] = nlohmann::json::array();
            std::cout << result.dump(2) << std::endl;
        } else {
            std::cout << "No backups found" << std::endl;
        }
        return 0;
    }
    
    auto backups_result = listBackups(backup_dir);
    if (!backups_result.has_value()) {
        return std::unexpected(makeError(ErrorCode::kFileError, backups_result.error().message()));
    }
    
    outputBackupList(*backups_result, options);
    return 0;
}

Result<int> BackupCommand::executeRestore() {
    const auto& options = app_.globalOptions();
    fs::path backup_path(backup_path_);
    
    if (!fs::exists(backup_path)) {
        return std::unexpected(makeError(ErrorCode::kFileNotFound, "Backup file not found: " + backup_path.string()));
    }
    
    // Default restore target to current notes directory
    fs::path target_dir;
    if (restore_path_.empty()) {
        target_dir = app_.config().notes_dir.parent_path();
    } else {
        target_dir = fs::path(restore_path_);
    }
    
    // Confirm restore operation
    if (!force_ && !options.json) {
        std::cout << "This will restore backup to: " << target_dir << std::endl;
        std::cout << "Continue? (y/N): ";
        std::string response;
        std::getline(std::cin, response);
        
        if (response != "y" && response != "Y") {
            std::cout << "Restore cancelled" << std::endl;
            return 1;
        }
    }
    
    outputProgress("Restoring backup: " + backup_path.filename().string(), options);
    
    auto restore_result = restoreFromBackup(backup_path, target_dir);
    if (!restore_result.has_value()) {
        return std::unexpected(makeError(ErrorCode::kFileError, restore_result.error().message()));
    }
    
    if (options.json) {
        nlohmann::json result;
        result["backup_path"] = backup_path.string();
        result["target_dir"] = target_dir.string();
        result["restored_at"] = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        std::cout << result.dump(2) << std::endl;
    } else {
        std::cout << "Backup restored successfully to: " << target_dir << std::endl;
    }
    
    return 0;
}

Result<int> BackupCommand::executeVerify() {
    const auto& options = app_.globalOptions();
    fs::path backup_path(backup_path_);
    
    if (!fs::exists(backup_path)) {
        return std::unexpected(makeError(ErrorCode::kFileNotFound, "Backup file not found: " + backup_path.string()));
    }
    
    outputProgress("Verifying backup: " + backup_path.filename().string(), options);
    
    auto verify_result = verifyBackup(backup_path);
    if (!verify_result.has_value()) {
        return std::unexpected(makeError(ErrorCode::kFileError, verify_result.error().message()));
    }
    
    bool is_valid = *verify_result;
    
    if (options.json) {
        nlohmann::json result;
        result["backup_path"] = backup_path.string();
        result["is_valid"] = is_valid;
        std::cout << result.dump(2) << std::endl;
    } else {
        if (is_valid) {
            std::cout << "Backup is valid" << std::endl;
        } else {
            std::cout << "Backup is corrupted or invalid" << std::endl;
        }
    }
    
    return is_valid ? 0 : 1;
}

Result<int> BackupCommand::executeCleanup() {
    const auto& options = app_.globalOptions();
    auto data_dir = app_.config().data_dir;
    auto backup_dir = data_dir / "backups";
    
    if (!fs::exists(backup_dir)) {
        if (options.json) {
            nlohmann::json result;
            result["removed_count"] = 0;
            std::cout << result.dump(2) << std::endl;
        } else {
            std::cout << "No backup directory found" << std::endl;
        }
        return 0;
    }
    
    auto backups_result = listBackups(backup_dir);
    if (!backups_result.has_value()) {
        return std::unexpected(makeError(ErrorCode::kFileError, backups_result.error().message()));
    }
    
    auto& backups = *backups_result;
    
    // Keep only the 5 most recent backups
    if (backups.size() <= 5) {
        if (options.json) {
            nlohmann::json result;
            result["removed_count"] = 0;
            std::cout << result.dump(2) << std::endl;
        } else {
            std::cout << "No old backups to remove" << std::endl;
        }
        return 0;
    }
    
    // Sort by creation time (newest first)
    std::sort(backups.begin(), backups.end(), [](const BackupInfo& a, const BackupInfo& b) {
        return a.created > b.created;
    });
    
    int to_remove = static_cast<int>(backups.size()) - 5;
    
    // Confirm cleanup
    if (!force_ && !options.json) {
        std::cout << "This will remove " << to_remove << " old backup(s)." << std::endl;
        std::cout << "Continue? (y/N): ";
        std::string response;
        std::getline(std::cin, response);
        
        if (response != "y" && response != "Y") {
            std::cout << "Cleanup cancelled" << std::endl;
            return 1;
        }
    }
    
    outputProgress("Cleaning up old backups...", options);
    
    int removed_count = 0;
    for (size_t i = 5; i < backups.size(); ++i) {
        if (fs::remove(backups[i].path)) {
            removed_count++;
        }
    }
    
    if (options.json) {
        nlohmann::json result;
        result["removed_count"] = removed_count;
        std::cout << result.dump(2) << std::endl;
    } else {
        std::cout << "Removed " << removed_count << " old backup(s)" << std::endl;
    }
    
    return 0;
}

Result<BackupCommand::BackupInfo> BackupCommand::createBackup(const fs::path& target_path) {
    // Create temporary directory for staging
    auto temp_dir = fs::temp_directory_path() / ("nx_backup_" + std::to_string(std::time(nullptr)));
    
    std::error_code ec;
    if (!fs::create_directories(temp_dir, ec) && ec) {
        return std::unexpected(makeError(ErrorCode::kDirectoryCreateError, "Failed to create temp directory: " + ec.message()));
    }
    
    BackupInfo info;
    info.path = target_path;
    info.created = std::chrono::system_clock::now();
    info.has_attachments = include_attachments_;
    info.has_index = include_index_;
    info.has_config = include_config_;
    info.compression = compression_;
    info.note_count = 0;
    
    // Copy notes directory
    auto notes_dir = app_.config().notes_dir;
    auto target_notes = temp_dir / "notes";
    
    if (fs::exists(notes_dir)) {
        fs::copy(notes_dir, target_notes, fs::copy_options::recursive);
        
        // Count notes
        for (const auto& entry : fs::recursive_directory_iterator(target_notes)) {
            if (entry.is_regular_file() && entry.path().extension() == ".md") {
                info.note_count++;
            }
        }
    }
    
    // Copy config if requested
    if (include_config_) {
        auto config_dir = app_.config().data_dir / ".." / "config";
        auto target_config = temp_dir / "config";
        if (fs::exists(config_dir)) {
            fs::copy(config_dir, target_config, fs::copy_options::recursive);
        }
    }
    
    // Copy index if requested
    if (include_index_) {
        auto cache_dir = app_.config().data_dir / ".." / "cache";
        auto target_cache = temp_dir / "cache";
        if (fs::exists(cache_dir)) {
            fs::copy(cache_dir, target_cache, fs::copy_options::recursive);
        }
    }
    
    // Copy attachments if requested
    if (include_attachments_) {
        auto data_dir = app_.config().data_dir;
        auto attachments_dir = data_dir / "attachments";
        auto target_attachments = temp_dir / "attachments";
        if (fs::exists(attachments_dir)) {
            fs::copy(attachments_dir, target_attachments, fs::copy_options::recursive);
        }
    }
    
    // Create tar.gz archive
    auto tar_result = createTarGz(temp_dir, target_path);
    
    // Cleanup temp directory
    fs::remove_all(temp_dir);
    
    if (!tar_result.has_value()) {
        return std::unexpected(makeError(ErrorCode::kExternalToolError, tar_result.error().message()));
    }
    
    // Get final backup size
    if (fs::exists(target_path)) {
        info.size_bytes = fs::file_size(target_path);
    }
    
    // Create metadata file alongside the backup
    auto metadata_path = target_path;
    metadata_path.replace_extension(metadata_path.extension().string() + ".meta.json");
    if (auto meta_result = writeBackupMetadata(metadata_path, info); !meta_result.has_value()) {
        // Log warning but don't fail the backup
        // The backup itself succeeded, metadata is optional
    }
    
    return info;
}

Result<BackupCommand::BackupInfo> BackupCommand::getBackupInfo(const fs::path& backup_path) {
    if (!fs::exists(backup_path)) {
        return std::unexpected(makeError(ErrorCode::kFileNotFound, "Backup file not found"));
    }
    
    BackupInfo info;
    info.path = backup_path;
    info.size_bytes = fs::file_size(backup_path);
    auto file_time = fs::last_write_time(backup_path);
    info.created = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        file_time - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
    
    // Try to read metadata file
    auto metadata_path = backup_path;
    metadata_path.replace_extension(metadata_path.extension().string() + ".meta.json");
    
    if (fs::exists(metadata_path)) {
        if (auto metadata = loadBackupMetadata(metadata_path); metadata.has_value()) {
            const auto& meta = *metadata;
            if (meta.contains("note_count")) info.note_count = meta["note_count"];
            if (meta.contains("has_attachments")) info.has_attachments = meta["has_attachments"];
            if (meta.contains("has_index")) info.has_index = meta["has_index"];
            if (meta.contains("has_config")) info.has_config = meta["has_config"];
            if (meta.contains("compression")) info.compression = meta["compression"];
        } else {
            // Fallback to defaults if metadata can't be read
            info.note_count = 0;
            info.has_attachments = true;
            info.has_index = false;
            info.has_config = true;
            info.compression = "gzip";
        }
    } else {
        // No metadata file, use defaults
        info.note_count = 0;
        info.has_attachments = true;
        info.has_index = false;
        info.has_config = true;
        info.compression = "gzip";
    }
    
    return info;
}

Result<std::vector<BackupCommand::BackupInfo>> BackupCommand::listBackups(const fs::path& backup_dir) {
    std::vector<BackupInfo> backups;
    
    for (const auto& entry : fs::directory_iterator(backup_dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".gz") {
            auto info_result = getBackupInfo(entry.path());
            if (info_result.has_value()) {
                backups.push_back(*info_result);
            }
        }
    }
    
    // Sort by creation time (newest first)
    std::sort(backups.begin(), backups.end(), [](const BackupInfo& a, const BackupInfo& b) {
        return a.created > b.created;
    });
    
    return backups;
}

Result<void> BackupCommand::restoreFromBackup(const fs::path& backup_path, const fs::path& target_dir) {
    return extractTarGz(backup_path, target_dir);
}

Result<bool> BackupCommand::verifyBackup(const fs::path& backup_path) {
    // Test archive integrity by attempting to list contents
    std::ostringstream cmd;
    cmd << "tar -tzf \"" << backup_path.string() << "\" >/dev/null 2>&1";
    
    int result = std::system(cmd.str().c_str());
    return result == 0;
}

Result<void> BackupCommand::createTarGz(const fs::path& source_dir, const fs::path& target_file) {
    std::ostringstream cmd;
    cmd << "tar -czf \"" << target_file.string() << "\" -C \"" << source_dir.string() << "\" .";
    
    int result = std::system(cmd.str().c_str());
    if (result != 0) {
        return std::unexpected(makeError(ErrorCode::kExternalToolError, "Failed to create tar.gz archive"));
    }
    
    return {};
}

Result<void> BackupCommand::extractTarGz(const fs::path& tar_file, const fs::path& target_dir) {
    std::error_code ec;
    if (!fs::create_directories(target_dir, ec) && ec) {
        return std::unexpected(makeError(ErrorCode::kDirectoryCreateError, "Failed to create target directory: " + ec.message()));
    }
    
    std::ostringstream cmd;
    cmd << "tar -xzf \"" << tar_file.string() << "\" -C \"" << target_dir.string() << "\"";
    
    int result = std::system(cmd.str().c_str());
    if (result != 0) {
        return std::unexpected(makeError(ErrorCode::kExternalToolError, "Failed to extract tar.gz archive"));
    }
    
    return {};
}

Result<std::string> BackupCommand::generateBackupName() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::ostringstream ss;
    ss << "backup-" << std::put_time(std::gmtime(&time_t), "%Y%m%d-%H%M%S");
    
    return ss.str();
}

void BackupCommand::outputBackupInfo(const BackupInfo& info, const GlobalOptions& options) {
    if (options.json) {
        nlohmann::json json_info;
        json_info["path"] = info.path.string();
        json_info["size_bytes"] = info.size_bytes;
        json_info["note_count"] = info.note_count;
        json_info["has_attachments"] = info.has_attachments;
        json_info["has_index"] = info.has_index;
        json_info["has_config"] = info.has_config;
        json_info["compression"] = info.compression;
        std::cout << json_info.dump(2) << std::endl;
    } else {
        std::cout << "Path: " << info.path << "\n";
        std::cout << "Size: " << formatFileSize(info.size_bytes) << "\n";
        std::cout << "Notes: " << info.note_count << "\n";
        std::cout << "Attachments: " << (info.has_attachments ? "yes" : "no") << "\n";
        std::cout << "Index: " << (info.has_index ? "yes" : "no") << "\n";
        std::cout << "Config: " << (info.has_config ? "yes" : "no") << std::endl;
    }
}

void BackupCommand::outputBackupList(const std::vector<BackupInfo>& backups, const GlobalOptions& options) {
    if (options.json) {
        nlohmann::json result;
        auto& backup_array = result["backups"];
        
        for (const auto& backup : backups) {
            nlohmann::json backup_json;
            backup_json["path"] = backup.path.string();
            backup_json["size_bytes"] = backup.size_bytes;
            backup_json["note_count"] = backup.note_count;
            backup_json["has_attachments"] = backup.has_attachments;
            backup_json["has_index"] = backup.has_index;
            backup_json["has_config"] = backup.has_config;
            backup_json["compression"] = backup.compression;
            backup_array.push_back(backup_json);
        }
        
        std::cout << result.dump(2) << std::endl;
    } else {
        if (backups.empty()) {
            std::cout << "No backups found" << std::endl;
        } else {
            std::cout << "Available backups:\n" << std::endl;
            for (const auto& backup : backups) {
                std::cout << backup.path.filename().string() << "\n";
                std::cout << "  Size: " << formatFileSize(backup.size_bytes) << "\n";
                std::cout << "  Notes: " << backup.note_count << "\n";
                std::cout << "  Created: " << formatTime(backup.created) << "\n" << std::endl;
            }
        }
    }
}

Result<void> BackupCommand::writeBackupMetadata(const fs::path& metadata_path, const BackupInfo& info) {
    nlohmann::json metadata;
    metadata["created_at"] = formatTime(info.created);
    metadata["note_count"] = info.note_count;
    metadata["has_attachments"] = info.has_attachments;
    metadata["has_index"] = info.has_index;
    metadata["has_config"] = info.has_config;
    metadata["compression"] = info.compression;
    metadata["size_bytes"] = info.size_bytes;
    
    std::ofstream file(metadata_path);
    if (!file) {
        return std::unexpected(makeError(ErrorCode::kFileWriteError, "Failed to write backup metadata"));
    }
    
    file << metadata.dump(2);
    return {};
}

Result<nlohmann::json> BackupCommand::loadBackupMetadata(const fs::path& metadata_path) {
    std::ifstream file(metadata_path);
    if (!file) {
        return std::unexpected(makeError(ErrorCode::kFileReadError, "Failed to read backup metadata"));
    }
    
    try {
        nlohmann::json metadata;
        file >> metadata;
        return metadata;
    } catch (const std::exception& e) {
        return std::unexpected(makeError(ErrorCode::kParseError, "Failed to parse backup metadata: " + std::string(e.what())));
    }
}

void BackupCommand::outputProgress(const std::string& message, const GlobalOptions& options) {
    if (!options.json) {
        std::cout << message << std::endl;
    }
}

std::string BackupCommand::formatFileSize(std::uintmax_t bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit = 0;
    double size = static_cast<double>(bytes);
    
    while (size >= 1024.0 && unit < 4) {
        size /= 1024.0;
        unit++;
    }
    
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(1) << size << " " << units[unit];
    return ss.str();
}

std::string BackupCommand::formatTime(const std::chrono::system_clock::time_point& tp) {
    auto time_t = std::chrono::system_clock::to_time_t(tp);
    std::ostringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%d %H:%M:%S UTC");
    return ss.str();
}

} // namespace nx::cli