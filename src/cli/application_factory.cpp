#include "nx/cli/application_factory.hpp"

#include "nx/di/service_configuration.hpp"

namespace nx::cli {

Result<std::unique_ptr<Application>> ApplicationFactory::createProductionApplication(
    const std::optional<std::filesystem::path>& config_path) {
    
    // Create service container with production configuration
    auto container_result = nx::di::ServiceContainerFactory::createProductionContainer(config_path);
    if (!container_result.has_value()) {
        return std::unexpected(container_result.error());
    }
    
    // Create application with the configured container
    auto app = std::make_unique<Application>(*container_result);
    
    // Initialize services
    auto init_result = app->initialize();
    if (!init_result.has_value()) {
        return std::unexpected(init_result.error());
    }
    
    return app;
}

Result<std::unique_ptr<Application>> ApplicationFactory::createTestApplication() {
    // Create service container with test configuration
    auto container_result = nx::di::ServiceContainerFactory::createTestContainer();
    if (!container_result.has_value()) {
        return std::unexpected(container_result.error());
    }
    
    // Create application with the test container
    auto app = std::make_unique<Application>(*container_result);
    
    // Initialize services
    auto init_result = app->initialize();
    if (!init_result.has_value()) {
        return std::unexpected(init_result.error());
    }
    
    return app;
}

std::unique_ptr<Application> ApplicationFactory::createWithContainer(
    std::shared_ptr<nx::di::IServiceContainer> container) {
    
    return std::make_unique<Application>(std::move(container));
}

} // namespace nx::cli