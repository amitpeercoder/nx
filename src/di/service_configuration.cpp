#include "nx/di/service_configuration.hpp"

#include "nx/store/filesystem_store.hpp"
#include "nx/store/filesystem_attachment_store.hpp"
#include "nx/index/sqlite_index.hpp"
#include "nx/index/ripgrep_index.hpp"
#include "nx/util/xdg.hpp"

namespace nx::di {

Result<void> ServiceConfiguration::configureServices(
    std::shared_ptr<IServiceContainer> container,
    const std::optional<std::filesystem::path>& config_path) {
    
    // Configure services in dependency order
    auto config_result = configureConfig(container, config_path);
    if (!config_result.has_value()) {
        return config_result;
    }
    
    auto storage_result = configureStorage(container);
    if (!storage_result.has_value()) {
        return storage_result;
    }
    
    auto indexing_result = configureIndexing(container);
    if (!indexing_result.has_value()) {
        return indexing_result;
    }
    
    auto templates_result = configureTemplates(container);
    if (!templates_result.has_value()) {
        return templates_result;
    }
    
    return {};
}

Result<void> ServiceConfiguration::configureTestServices(
    std::shared_ptr<IServiceContainer> container) {
    
    // For testing, we would register mock implementations
    // This is a placeholder for future test infrastructure
    
    return configureServices(container);
}

Result<void> ServiceConfiguration::configureConfig(
    std::shared_ptr<IServiceContainer> container,
    const std::optional<std::filesystem::path>& config_path) {
    
    container->registerFactory<nx::config::Config>(
        [config_path]() -> std::shared_ptr<nx::config::Config> {
            auto config = std::make_shared<nx::config::Config>();
            
            if (config_path.has_value()) {
                auto load_result = config->load(*config_path);
                if (!load_result.has_value()) {
                    // Log warning but don't fail - use defaults
                }
            } else {
                // Try to load default config file
                auto default_config_path = nx::util::Xdg::configFile();
                if (std::filesystem::exists(default_config_path)) {
                    auto load_result = config->load(default_config_path);
                    if (!load_result.has_value()) {
                        // Log warning but don't fail - use defaults
                    }
                }
            }
            
            return config;
        },
        ServiceLifetime::Singleton
    );
    
    return {};
}

Result<void> ServiceConfiguration::configureStorage(
    std::shared_ptr<IServiceContainer> container) {
    
    // Register NoteStore
    container->registerFactory<nx::store::NoteStore>(
        [container]() -> std::shared_ptr<nx::store::NoteStore> {
            auto config = container->resolve<nx::config::Config>();
            
            nx::store::FilesystemStore::Config store_config;
            store_config.notes_dir = nx::util::Xdg::notesDir();
            store_config.attachments_dir = nx::util::Xdg::attachmentsDir();
            store_config.trash_dir = nx::util::Xdg::trashDir();
            store_config.auto_create_dirs = true;
            store_config.validate_paths = true;
            
            return std::make_shared<nx::store::FilesystemStore>(store_config);
        },
        ServiceLifetime::Singleton
    );
    
    // Register AttachmentStore
    container->registerFactory<nx::store::AttachmentStore>(
        [container]() -> std::shared_ptr<nx::store::AttachmentStore> {
            auto config = container->resolve<nx::config::Config>();
            
            nx::store::FilesystemAttachmentStore::Config store_config;
            store_config.attachments_dir = nx::util::Xdg::attachmentsDir();
            store_config.metadata_file = nx::util::Xdg::attachmentsDir() / "metadata.json";
            store_config.auto_create_dirs = true;
            
            return std::make_shared<nx::store::FilesystemAttachmentStore>(store_config);
        },
        ServiceLifetime::Singleton
    );
    
    // Register NotebookManager
    container->registerFactory<nx::store::NotebookManager>(
        [container]() -> std::shared_ptr<nx::store::NotebookManager> {
            auto note_store = container->resolve<nx::store::NoteStore>();
            
            return std::make_shared<nx::store::NotebookManager>(*note_store);
        },
        ServiceLifetime::Singleton
    );
    
    return {};
}

Result<void> ServiceConfiguration::configureIndexing(
    std::shared_ptr<IServiceContainer> container) {
    
    // Register Index (choose SQLite as primary, with ripgrep fallback)
    container->registerFactory<nx::index::Index>(
        [container]() -> std::shared_ptr<nx::index::Index> {
            auto config = container->resolve<nx::config::Config>();
            
            // Try SQLite index first
            try {
                auto db_path = nx::util::Xdg::indexFile();
                auto sqlite_index = std::make_shared<nx::index::SqliteIndex>(db_path);
                
                auto init_result = sqlite_index->initialize();
                if (init_result.has_value()) {
                    return sqlite_index;
                }
            } catch (...) {
                // Fall through to ripgrep
            }
            
            // Fallback to ripgrep index
            auto notes_dir = nx::util::Xdg::notesDir();
            auto ripgrep_index = std::make_shared<nx::index::RipgrepIndex>(notes_dir);
            
            auto init_result = ripgrep_index->initialize();
            if (!init_result.has_value()) {
                throw ServiceResolutionException("Failed to initialize any search index");
            }
            
            return ripgrep_index;
        },
        ServiceLifetime::Singleton
    );
    
    return {};
}

Result<void> ServiceConfiguration::configureTemplates(
    std::shared_ptr<IServiceContainer> container) {
    
    // Register TemplateManager
    container->registerFactory<nx::template_system::TemplateManager>(
        [container]() -> std::shared_ptr<nx::template_system::TemplateManager> {
            auto config = container->resolve<nx::config::Config>();
            
            nx::template_system::TemplateManager::Config template_config;
            template_config.templates_dir = nx::util::Xdg::configHome() / "templates";
            template_config.metadata_file = nx::util::Xdg::configHome() / "templates" / "metadata.json";
            
            return std::make_shared<nx::template_system::TemplateManager>(template_config);
        },
        ServiceLifetime::Singleton
    );
    
    return {};
}

Result<std::shared_ptr<IServiceContainer>> ServiceContainerFactory::createProductionContainer(
    const std::optional<std::filesystem::path>& config_path) {
    
    auto container = std::make_shared<ServiceContainer>();
    
    auto configure_result = ServiceConfiguration::configureServices(container, config_path);
    if (!configure_result.has_value()) {
        return std::unexpected(configure_result.error());
    }
    
    return container;
}

Result<std::shared_ptr<IServiceContainer>> ServiceContainerFactory::createTestContainer() {
    auto container = std::make_shared<ServiceContainer>();
    
    auto configure_result = ServiceConfiguration::configureTestServices(container);
    if (!configure_result.has_value()) {
        return std::unexpected(configure_result.error());
    }
    
    return container;
}

} // namespace nx::di