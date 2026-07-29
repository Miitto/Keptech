#pragma once

#include "keptech/core/scene.hpp"

namespace kt {
  Scene Scene::s_active{};
  ecs::Ecs& Scene::getEcs() { return ecs; }
  [[nodiscard]] const ecs::Ecs& Scene::getEcs() const { return ecs; }
  ecs::Entity Scene::createEntity(const std::string& name) {
    ecs::Entity entity = {ecs.create(), ecs};
    if (!name.empty()) {
      entity.addComponent<components::Name>(name);
    }
    return entity;
  }
  void Scene::useCamera(ecs::EntityHandle cameraEntity) { activeCamera = cameraEntity; }
  [[nodiscard]] ecs::Entity Scene::getActiveCamera() { return {activeCamera, ecs}; }

  Scene Scene::setActive(Scene&& scene) {
    Scene previousActive = std::move(s_active);
    s_active = std::move(scene);
    return previousActive;
  }
  Scene& Scene::active() { return s_active; }
} // namespace kt