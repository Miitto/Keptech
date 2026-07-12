#pragma once

#include "keptech/core/scene.hpp"
#include "keptech/ecs/entity.hpp"
#include "keptech/maths/transform.hpp"
#include "keptech/rendering/material.hpp"
#include "keptech/rendering/mesh.hpp"
#include <fastgltf/types.hpp>
#include <string>

namespace kt::gltf {
  struct Scene {
    struct Node {
      std::string name;
      maths::Transform transform;
      Mesh mesh;
      rendering::Material material;
      std::vector<Node> children{};
    };

    std::vector<Node> roots;
    std::vector<Mesh> meshes;
    std::vector<rendering::Material> materials;

    void addToEcsScene(kt::Scene& scene, kt::ecs::EntityHandle parent) const;
  };
} // namespace kt::gltf
