#include "scene.hpp"
#include "keptech/components/transform.hpp"

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

  void Scene::clear() {
    ecs.clear();
    activeCamera = ecs::INVALID_ENTITY_HANDLE;
  }
  Scene& Scene::active() { return s_active; }
} // namespace kt