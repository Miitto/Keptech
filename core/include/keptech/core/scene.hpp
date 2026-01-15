#pragma once

#include <keptech/core/components/name.hpp>
#include <keptech/ecs/entity.hpp>

namespace keptech::core {
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

  private:
    ecs::Ecs ecs{};
    ecs::EntityHandle activeCamera;
  };
} // namespace keptech::core
