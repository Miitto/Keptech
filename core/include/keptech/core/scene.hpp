#pragma once

#include <keptech/core/components/name.hpp>
#include <keptech/ecs/entity.hpp>

namespace keptech {
  class Scene {
  public:
    ecs::Ecs& getEcs() { return ecs; }
    [[nodiscard]] const ecs::Ecs& getEcs() const { return ecs; }

    ecs::Entity createEntity(const std::string& name = "") {
      ecs::Entity entity = {ecs.create(), ecs};
      if (!name.empty()) {
        entity.addComponent<components::Name>(name);
      }
      return entity;
    }

    void useCamera(ecs::EntityHandle cameraEntity) {
      activeCamera = cameraEntity;
    }

    [[nodiscard]] ecs::Entity getActiveCamera() { return {activeCamera, ecs}; }

    template <typename Component, typename FRef>
    static void registerComponentFunctions(const char* functionName) {
      entt::meta_factory<Component>().template func<FRef>(
          entt::hashed_string(functionName));
    }

  private:
    ecs::Ecs ecs{};
    ecs::EntityHandle activeCamera;
  };
} // namespace keptech
