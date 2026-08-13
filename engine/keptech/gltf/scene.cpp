#include "keptech/rhi/gltf/scene.hpp"

#include "keptech/components/transform.hpp"
#include "keptech/core/kt-logger.hpp"
#include "keptech/rhi/gltf/data.hpp"

namespace kt::gltf {
  namespace {
    void addNodeToEcsScene(const Scene::Node& node, kt::Scene& scene, ecs::EntityHandle parent) {
      auto entity = scene.createEntity(node.name);
      auto& transformComp = entity.addComponent<components::Transform>();
      transformComp.getLocalMut().setPosition(node.transform.pos()).setRotation(node.transform.rot()).setScale(node.transform.scale());

      if (parent != ecs::INVALID_ENTITY_HANDLE) {
        transformComp.setParent(entity, {parent, scene.getEcs()});
      }

      if (node.mesh.isValid()) {
        entity.addComponent<components::Mesh>(node.mesh);
      }

      for (auto& child : node.children) {
        addNodeToEcsScene(child, scene, entity.getHandle());
      }
    }

    gltf::Scene::Node createNode(const Data::Node& node, const std::vector<Mesh>& meshes) {
      gltf::Scene::Node result{
          .name = std::string(node.node.name),
          .transform = node.transform,
          .mesh = node.meshIndex == ~0u ? Mesh() : meshes[node.meshIndex],
      };
      for (auto& child : node.children) {
        result.children.push_back(createNode(child, meshes));
      }

      return result;
    }
  } // namespace

  Scene::Scene(const gltf::Data& data, const std::vector<Mesh>& meshes) {
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
      addNodeToEcsScene(root, scene, parent);
    }
  }
} // namespace kt::gltf
