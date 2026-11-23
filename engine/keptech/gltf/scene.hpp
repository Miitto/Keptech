#pragma once

#include "keptech/core/scene.hpp"
#include "keptech/ecs/entity.hpp"
#include "keptech/material.hpp"
#include "keptech/maths/transform.hpp"
#include "keptech/mesh.hpp"
#include "keptech/rhi/sync.hpp"
#include "keptech/std/vec.hpp"
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
      Vec<Node> children{};
    };

    Vec<Node> roots;
    Vec<Mesh> meshes;
    Vec<Material> materials;

    rhi::CopyWaitValue copyFenceValue = 0;

    Scene(const gltf::Data& data, const Vec<Mesh>& meshes, rhi::CopyWaitValue copyFenceValue);

    void addToEcsScene(kt::Scene& scene, kt::ecs::EntityHandle parent) const;
  };
} // namespace kt::gltf
