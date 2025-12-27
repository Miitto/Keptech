#pragma once

#include "base.hpp"
#include "componentManager.hpp"
#include "entityManager.hpp"
#include "systemManager.hpp"
#include <memory>

namespace keptech::ecs {
  class ECS {
    explicit ECS() = default;

  public:
    /// Returns the singleton instance of the ECS.
    static ECS& get() { return singleton; }

#pragma region Entity Methods
    /// Creates a new entity with the given name.
    Entity& createEntity(const std::string& name) {
      return entityManager->create(name);
    }

    /// Destroys the given entity and removes all its components.
    void destroyEntity(EntityHandle entity) {
      entityManager->destroy(entity);
      componentManager->onEntityDestroyed(entity);
      systemManager->onEntityDestroyed(entity);
    }

    bool hasEntity(EntityHandle entity) {
      try {
        entityManager->get(entity);
        return true;
      } catch (...) {
        return false;
      }
    }

    Entity* getEntity(EntityHandle entity) {
      return entityManager->get(entity);
    }

    Entity& getEntityRef(EntityHandle entity) {
      return entityManager->at(entity);
    }
#pragma endregion

#pragma region Component Methods
    /// Adds a component of type T to the given entity.
    template <typename T>
    void addComponent(EntityHandle entity, T&& component) {
      componentManager->add<T>(entity, std::forward<T>(component));

      auto componentType = componentManager->getComponentType<T>();
      auto& entitySig = entityManager->at(entity);
      entityManager->at(entity).getSignature().set(componentType, true);
      systemManager->onEntitySignatureChanged(entity, entitySig.getSignature());
    }

    /// Removes the component of type T from the given entity.
    template <typename T> void removeComponent(EntityHandle entity) {
      componentManager->remove<T>(entity);

      auto componentType = componentManager->getComponentType<T>();
      auto& entitySig = entityManager->at(entity);
      entityManager->at(entity).getSignature().set(componentType, false);
      systemManager->onEntitySignatureChanged(entity, entitySig.getSignature());
    }

    /// Checks if the given entity has a component of type T.
    template <typename T> bool hasComponent(EntityHandle entity) {
      return componentManager->has<T>(entity);
    }

    /// Gets a pointer to the component of type T for the given entity.
    /// Returns nullptr if the entity does not have the component.
    template <typename T> T* getComponent(EntityHandle entity) {
      return componentManager->get<T>(entity);
    }

    /// Gets a reference to the component of type T for the given entity.
    /// Asserts that the entity has the component.
    template <typename T> T& getComponentRef(EntityHandle entity) {
      return componentManager->at<T>(entity);
    }

    /// Gets the ComponentType for the component type T.
    template <typename T> ComponentType getComponentType() {
      return componentManager->getComponentType<T>();
    }

    /// Gets a Signature that includes all the given component types.
    template <typename... Args> Signature signatureFromComponents() {
      Signature signature;
      (signature.set(getComponentType<Args>(), true), ...);
      return signature;
    }
#pragma endregion

#pragma region System Methods
    /// Checks if a system of type T is registered.
    template <typename T> bool hasSystem() { return systemManager->has<T>(); }
    /// Gets a reference to the system of type T.
    /// Asserts that the system is registered.
    template <typename T> T& getSystem() { return systemManager->get<T>(); }

    /// Registers a system of type T with the given signature using the given
    /// constructor arguments.
    template <typename T, typename... Args>
    T& registerSystem(Signature signature, Args&&... args) {
      return systemManager->registerSystem<T>(signature,
                                              std::forward<Args>(args)...);
    }

    /// Sets the signature for the system of type T.
    /// WARNING: Changing the signature of a system at runtime will not register
    /// entities that currently match the new signature.
    template <typename T> void setSystemSignature(Signature signature) {
      systemManager->setSignature<T>(signature);
    }

    inline void preUpdateAllSystems(const FrameData& frameData) {
      systemManager->preUpdateAllSystems(frameData);
    }

    inline void updateAllSystems(const FrameData& frameData) {
      systemManager->updateAllSystems(frameData);
    }

    inline void postUpdateAllSystems(const FrameData& frameData) {
      systemManager->postUpdateAllSystems(frameData);
    }
#pragma endregion

    void destroy() {
      entityManager.reset();
      componentManager.reset();
      systemManager.reset();
    }

  private:
    static ECS singleton;

    std::unique_ptr<EntityManager> entityManager =
        std::make_unique<EntityManager>();
    std::unique_ptr<ComponentManager> componentManager =
        std::make_unique<ComponentManager>();
    std::unique_ptr<SystemManager> systemManager =
        std::make_unique<SystemManager>();
  };
} // namespace keptech::ecs
