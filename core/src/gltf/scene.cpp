#include "keptech/core/rendering/gltf/scene.hpp"

#include "keptech/core/components/renderObject.hpp"
#include "keptech/core/components/transform.hpp"

#include "keptech/core/kt-logger.hpp"

namespace keptech::gltf {
  namespace {
    void addNodeToEcsScene(const Scene::Node& node, keptech::Scene& scene,
                           ecs::EntityHandle parent) {
      auto entity = scene.createEntity(node.name);
      auto& transformComp = entity.addComponent<components::Transform>();
      transformComp.getLocalMut()
          .setPosition(node.transform.pos())
          .setRotation(node.transform.rot())
          .setScale(node.transform.scale());

      if (parent != ecs::INVALID_ENTITY_HANDLE) {
        transformComp.setParent({parent, scene.getEcs()});
      }

      if (node.mesh != nullptr) {
        entity.addComponent<components::Mesh>(node.mesh);
      }
      if (node.material != nullptr) {
        entity.addComponent<components::Material>(node.material);
      }

      for (auto& child : node.children) {
        addNodeToEcsScene(child, scene, entity.getHandle());
      }
    }
  } // namespace

  void Scene::addToEcsScene(keptech::Scene& scene,
                            ecs::EntityHandle parent) const {
    KT_DEBUG("Adding glTF scene to ECS scene with {} roots", roots.size());

    if (parent != ecs::INVALID_ENTITY_HANDLE) {
      ecs::Entity entity = {parent, scene.getEcs()};
      if (!entity.hasAllComponents<components::Transform>()) {
        entity.addComponent<components::Transform>();
      }
    }

    for (auto& root : roots) {
      addNodeToEcsScene(root, scene, parent);
    }
  }
} // namespace keptech::gltf
