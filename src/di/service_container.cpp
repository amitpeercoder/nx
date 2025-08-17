#include "nx/di/service_container.hpp"

namespace nx::di {

// Static member definition
std::shared_ptr<IServiceContainer> ServiceLocator::instance_ = nullptr;

void ServiceContainer::registerServiceImpl(std::type_index type,
                                         std::function<std::shared_ptr<void>()> factory,
                                         ServiceLifetime lifetime) {
    services_[type] = ServiceDescriptor{
        .factory = std::move(factory),
        .lifetime = lifetime,
        .singleton_instance = nullptr
    };
}

void ServiceContainer::registerInstanceImpl(std::type_index type,
                                          std::shared_ptr<void> instance) {
    services_[type] = ServiceDescriptor{
        .factory = nullptr,
        .lifetime = ServiceLifetime::Singleton,
        .singleton_instance = std::move(instance)
    };
}

std::shared_ptr<void> ServiceContainer::resolveImpl(std::type_index type) {
    auto it = services_.find(type);
    if (it == services_.end()) {
        throw ServiceResolutionException(
            "Service not registered: " + std::string(type.name()));
    }
    
    ServiceDescriptor& descriptor = it->second;
    
    // If we have a singleton instance, return it
    if (descriptor.singleton_instance) {
        return descriptor.singleton_instance;
    }
    
    // Create new instance via factory
    if (!descriptor.factory) {
        throw ServiceResolutionException(
            "No factory available for service: " + std::string(type.name()));
    }
    
    auto instance = descriptor.factory();
    
    // Cache singleton instances
    if (descriptor.lifetime == ServiceLifetime::Singleton) {
        descriptor.singleton_instance = instance;
    }
    
    return instance;
}

bool ServiceContainer::isRegisteredImpl(std::type_index type) const {
    return services_.find(type) != services_.end();
}

} // namespace nx::di