#pragma once

#include <memory>
#include <filesystem>
#include <optional>

#include "nx/common.hpp"
#include "nx/cli/application.hpp"
#include "nx/di/service_container.hpp"

namespace nx::cli {

/**
 * @brief Factory for creating properly configured Application instances
 */
class ApplicationFactory {
public:
    /**
     * @brief Create a production application with full service configuration
     * @param config_path Optional path to config file
     * @return Application instance ready for use
     */
    static Result<std::unique_ptr<Application>> createProductionApplication(
        const std::optional<std::filesystem::path>& config_path = std::nullopt);
    
    /**
     * @brief Create a test application with mock services
     * @return Application instance configured for testing
     */
    static Result<std::unique_ptr<Application>> createTestApplication();
    
    /**
     * @brief Create an application with a pre-configured service container
     * @param container Service container with all dependencies registered
     * @return Application instance using the provided container
     */
    static std::unique_ptr<Application> createWithContainer(
        std::shared_ptr<nx::di::IServiceContainer> container);
    
private:
    ApplicationFactory() = delete;
};

} // namespace nx::cli