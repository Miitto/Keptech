#pragma once

#include "keptech/core/components/lights.hpp"
#include "keptech/core/maths/transform.hpp"
#include "keptech/core/rendering/mesh.hpp"
#include "keptech/core/rendering/pipeline.hpp"
#include "keptech/core/scene.hpp"
#include "keptech/ecs/entity.hpp"
#include <fastgltf/types.hpp>
#include <string>

namespace kt::gltf {
  struct Scene {
    struct Node {
      std::string name;
      maths::Transform transform;
      MeshPtr mesh;
      MaterialPtr material;
      std::vector<Node> children{};
      std::optional<components::PointLight> pointLight;
    };

    std::vector<Node> roots;
    std::vector<MeshPtr> meshes;

    void addToEcsScene(Scene& scene, ecs::EntityHandle parent) const;
  };
} // namespace kt::gltf
