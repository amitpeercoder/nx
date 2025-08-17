#pragma once

#include <functional>
#include <memory>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <type_traits>

namespace nx::di {

/**
 * @brief Service lifetime options
 */
enum class ServiceLifetime {
    Singleton,  // Single instance for the entire application lifetime
    Transient,  // New instance created each time
    Scoped      // Single instance per scope (future extension)
};

/**
 * @brief Exception thrown when service resolution fails
 */
class ServiceResolutionException : public std::exception {
public:
    explicit ServiceResolutionException(const std::string& message) 
        : message_(message) {}
    
    const char* what() const noexcept override {
        return message_.c_str();
    }

private:
    std::string message_;
};

/**
 * @brief Interface for service registration and resolution
 */
class IServiceContainer {
public:
    virtual ~IServiceContainer() = default;
    
    /**
     * @brief Register a service factory function
     */
    template<typename TInterface, typename TImplementation>
    void registerService(ServiceLifetime lifetime = ServiceLifetime::Singleton) {
        static_assert(std::is_base_of_v<TInterface, TImplementation>, 
                     "TImplementation must inherit from TInterface");
        
        registerServiceImpl(
            std::type_index(typeid(TInterface)),
            [this]() -> std::shared_ptr<void> {
                return std::make_shared<TImplementation>(resolveInternal<TImplementation>());
            },
            lifetime
        );
    }
    
    /**
     * @brief Register a service with a custom factory function
     */
    template<typename TInterface>
    void registerFactory(std::function<std::shared_ptr<TInterface>()> factory,
                         ServiceLifetime lifetime = ServiceLifetime::Singleton) {
        registerServiceImpl(
            std::type_index(typeid(TInterface)),
            [factory]() -> std::shared_ptr<void> {
                return std::static_pointer_cast<void>(factory());
            },
            lifetime
        );
    }
    
    /**
     * @brief Register a singleton instance
     */
    template<typename TInterface>
    void registerInstance(std::shared_ptr<TInterface> instance) {
        registerInstanceImpl(std::type_index(typeid(TInterface)), 
                           std::static_pointer_cast<void>(instance));
    }
    
    /**
     * @brief Resolve a service instance
     */
    template<typename T>
    std::shared_ptr<T> resolve() {
        auto service = resolveImpl(std::type_index(typeid(T)));
        auto result = std::static_pointer_cast<T>(service);
        if (!result) {
            throw ServiceResolutionException(
                "Failed to resolve service: " + std::string(typeid(T).name()));
        }
        return result;
    }
    
    /**
     * @brief Check if a service is registered
     */
    template<typename T>
    bool isRegistered() const {
        return isRegisteredImpl(std::type_index(typeid(T)));
    }

protected:
    virtual void registerServiceImpl(std::type_index type,
                                   std::function<std::shared_ptr<void>()> factory,
                                   ServiceLifetime lifetime) = 0;
    
    virtual void registerInstanceImpl(std::type_index type,
                                    std::shared_ptr<void> instance) = 0;
    
    virtual std::shared_ptr<void> resolveImpl(std::type_index type) = 0;
    
    virtual bool isRegisteredImpl(std::type_index type) const = 0;

private:
    template<typename T>
    T resolveInternal() {
        // For constructor injection, we would analyze constructor parameters here
        // For now, use default constructor
        return T();
    }
};

/**
 * @brief Simple dependency injection container implementation
 */
class ServiceContainer : public IServiceContainer {
public:
    ServiceContainer() = default;
    ~ServiceContainer() = default;
    
    // Non-copyable
    ServiceContainer(const ServiceContainer&) = delete;
    ServiceContainer& operator=(const ServiceContainer&) = delete;
    
    // Movable
    ServiceContainer(ServiceContainer&&) = default;
    ServiceContainer& operator=(ServiceContainer&&) = default;

protected:
    void registerServiceImpl(std::type_index type,
                           std::function<std::shared_ptr<void>()> factory,
                           ServiceLifetime lifetime) override;
    
    void registerInstanceImpl(std::type_index type,
                            std::shared_ptr<void> instance) override;
    
    std::shared_ptr<void> resolveImpl(std::type_index type) override;
    
    bool isRegisteredImpl(std::type_index type) const override;

private:
    struct ServiceDescriptor {
        std::function<std::shared_ptr<void>()> factory;
        ServiceLifetime lifetime;
        std::shared_ptr<void> singleton_instance;
    };
    
    std::unordered_map<std::type_index, ServiceDescriptor> services_;
};

/**
 * @brief Service locator for global access to DI container
 * Note: This is provided for compatibility but dependency injection 
 * through constructors is preferred
 */
class ServiceLocator {
public:
    static void setContainer(std::shared_ptr<IServiceContainer> container) {
        instance_ = container;
    }
    
    static std::shared_ptr<IServiceContainer> getContainer() {
        if (!instance_) {
            throw ServiceResolutionException("ServiceLocator not initialized");
        }
        return instance_;
    }
    
    template<typename T>
    static std::shared_ptr<T> resolve() {
        return getContainer()->resolve<T>();
    }

private:
    static std::shared_ptr<IServiceContainer> instance_;
};

} // namespace nx::di