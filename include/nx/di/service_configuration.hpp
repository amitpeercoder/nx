#pragma once

#include <memory>
#include <filesystem>

#include "nx/di/service_container.hpp"
#include "nx/config/config.hpp"
#include "nx/store/note_store.hpp"
#include "nx/store/notebook_manager.hpp"
#include "nx/store/attachment_store.hpp"
#include "nx/index/index.hpp"
#include "nx/template/template_manager.hpp"

namespace nx::di {

/**
 * @brief Configuration class for setting up dependency injection services
 */
class ServiceConfiguration {
public:
    /**
     * @brief Configure all application services in the container
     * @param container The DI container to configure
     * @param config_path Optional path to configuration file
     * @return Result indicating success or failure
     */
    static Result<void> configureServices(
        std::shared_ptr<IServiceContainer> container,
        const std::optional<std::filesystem::path>& config_path = std::nullopt);

    /**
     * @brief Configure services for testing with mocks
     * @param container The DI container to configure
     * @return Result indicating success or failure
     */
    static Result<void> configureTestServices(
        std::shared_ptr<IServiceContainer> container);

private:
    static Result<void> configureConfig(
        std::shared_ptr<IServiceContainer> container,
        const std::optional<std::filesystem::path>& config_path);
    
    static Result<void> configureStorage(
        std::shared_ptr<IServiceContainer> container);
    
    static Result<void> configureIndexing(
        std::shared_ptr<IServiceContainer> container);
    
    static Result<void> configureTemplates(
        std::shared_ptr<IServiceContainer> container);
};

/**
 * @brief Factory for creating configured service containers
 */
class ServiceContainerFactory {
public:
    /**
     * @brief Create a fully configured service container for production
     */
    static Result<std::shared_ptr<IServiceContainer>> createProductionContainer(
        const std::optional<std::filesystem::path>& config_path = std::nullopt);
    
    /**
     * @brief Create a service container configured for testing
     */
    static Result<std::shared_ptr<IServiceContainer>> createTestContainer();
};

} // namespace nx::di