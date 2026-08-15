#pragma once

#include "keptech/core/scene.hpp"
#include "keptech/ecs/entity.hpp"
#include "keptech/material.hpp"
#include "keptech/maths/transform.hpp"
#include "keptech/mesh.hpp"
#include <fastgltf/types.hpp>
#include <string>

namespace kt::gltf {
  struct Data;

  struct Scene {
    struct Node {
      std::string name;
      maths::Transform transform;
      uint32_t meshIndex;
      uint32_t materialIndex;
      std::vector<Node> children{};
    };

    std::vector<Node> roots;
    std::vector<Mesh> meshes;
    std::vector<Material> materials;

    uint64_t copyFenceValue = 0;

    Scene(const gltf::Data& data, const std::vector<Mesh>& meshes, uint64_t copyFenceValue);

    void addToEcsScene(kt::Scene& scene, kt::ecs::EntityHandle parent) const;
  };
} // namespace kt::gltf
