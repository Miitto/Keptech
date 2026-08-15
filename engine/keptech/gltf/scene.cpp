#include "scene.hpp"

#include "data.hpp"
#include "keptech/components/transform.hpp"
#include "keptech/core/kt-logger.hpp"

namespace kt::gltf {
  namespace {
    void addNodeToEcsScene(const Scene::Node& node, kt::Scene& scene, const std::vector<Mesh>& meshes, ecs::EntityHandle parent) {
      auto entity = scene.createEntity(node.name);
      auto& transformComp = entity.addComponent<components::Transform>();
      transformComp.getLocalMut().setPosition(node.transform.pos()).setRotation(node.transform.rot()).setScale(node.transform.scale());

      if (parent != ecs::INVALID_ENTITY_HANDLE) {
        transformComp.setParent(entity, {parent, scene.getEcs()});
      }

      if (node.meshIndex != ~0u) {
        entity.addComponent<Mesh>(meshes[node.meshIndex]);
      }

      for (auto& child : node.children) {
        addNodeToEcsScene(child, scene, meshes, entity.getHandle());
      }
    }

    gltf::Scene::Node createNode(const Data::Node& node, const std::vector<Mesh>& meshes) {
      gltf::Scene::Node result{
          .name = std::string(node.node.name),
          .transform = node.transform,
          .meshIndex = node.meshIndex,
      };
      for (auto& child : node.children) {
        result.children.push_back(createNode(child, meshes));
      }

      return result;
    }
  } // namespace

  Scene::Scene(const gltf::Data& data, const std::vector<Mesh>& meshes, uint64_t copyFenceValue)
      : meshes(meshes), copyFenceValue(copyFenceValue) {
    roots.reserve(data.roots.size());
    for (auto& node : data.roots) {
      roots.push_back(createNode(node, meshes));
    }
  }

  void Scene::addToEcsScene(kt::Scene& scene, ecs::EntityHandle parent) const {
    KT_DEBUG("Adding glTF scene to ECS scene with {} roots", roots.size());

    if (parent != ecs::INVALID_ENTITY_HANDLE) {
      ecs::Entity entity = {parent, scene.getEcs()};
      if (!entity.hasAllComponents<components::Transform>()) {
        entity.addComponent<components::Transform>();
      }
    }

    for (auto& root : roots) {
      addNodeToEcsScene(root, scene, meshes, parent);
    }
  }
} // namespace kt::gltf
