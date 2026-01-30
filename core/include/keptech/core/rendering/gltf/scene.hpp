#pragma once

#include "keptech/core/maths/transform.hpp"
#include "keptech/core/rendering/mesh.hpp"
#include "keptech/core/rendering/pipeline.hpp"
#include "keptech/core/scene.hpp"
#include "keptech/ecs/entity.hpp"
#include <string>

namespace keptech::gltf {
  struct Scene {
    struct Node {
      std::string name;
      maths::Transform transform;
      MeshPtr mesh;
      MaterialPtr material;
      std::vector<Node> children{};
    };

    std::vector<Node> roots;
    std::vector<MeshPtr> meshes;

    void addToEcsScene(keptech::Scene& scene, ecs::EntityHandle parent) const;
  };
} // namespace keptech::gltf
