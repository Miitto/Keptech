#pragma once

#include "ecs-logger.hpp"
#include <entt/entt.hpp>

namespace kt::ecs {
  using EntityHandle = entt::entity;
  inline constexpr EntityHandle INVALID_ENTITY_HANDLE = entt::null;

  using Ecs = entt::registry;

  template <typename T> using MetaFactory = entt::meta_factory<T>;

  class Entity {
  public:
    Entity() = default;
    Entity(EntityHandle handle, entt::registry& ecs) : handle(handle), ecs(&ecs) {}

    operator EntityHandle() const { return handle; }
    [[nodiscard]] EntityHandle getHandle() const { return handle; }
    [[nodiscard]] bool isValid() const { return handle != INVALID_ENTITY_HANDLE && ecs != nullptr && ecs->valid(handle); }

    [[nodiscard]] Ecs& getEcs() const {
      ECS_ASSERT(ecs != nullptr, "Entity is not associated with any ECS");
      return *ecs;
    }

    template <typename C, typename... Args> auto& addComponent(Args&&... args) const {
      return ecs->emplace<C>(handle, std::forward<Args>(args)...);
    }

    template <typename C> void eraseComponent() const { ecs->erase<C>(handle); }

    template <typename C> void removeComponent() const { ecs->remove<C>(handle); }

    template <typename... C> [[nodiscard]] bool hasAllComponents() const { return ecs->all_of<C...>(handle); }

    template <typename... C> [[nodiscard]] bool hasAnyComponent() const { return ecs->any_of<C...>(handle); }

    template <typename... C> [[nodiscard]] decltype(auto) getComponents() const {
      ECS_ASSERT(hasAllComponents<C...>(), "Entity does not have the requested component");
      return ecs->get<C...>(handle);
    }

    void destroy() {
      ECS_ASSERT(isValid(), "Cannot destroy an invalid entity");
      ecs->destroy(handle);
      handle = INVALID_ENTITY_HANDLE;
      ecs = nullptr;
    }

  private:
    EntityHandle handle = INVALID_ENTITY_HANDLE;
    entt::registry* ecs = nullptr;
  };
} // namespace kt::ecs
