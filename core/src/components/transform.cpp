#include "keptech/core/components/transform.hpp"

#include "keptech/core/ecs/ecs.hpp"

namespace keptech::components {
  void Transform::recalculateGlobalTransform() {
    if (!dirty) {
      return;
    }

    global = local;
    if (parent == ecs::INVALID_ENTITY_HANDLE) {
      return;
    }

    auto& ecs = ecs::ECS::get();

    auto& parentTransform = ecs.getComponentRef<Transform>(parent);
    parentTransform.recalculateGlobalTransform();

    global.apply(parentTransform.global);
  }
} // namespace keptech::components
