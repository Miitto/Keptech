#pragma once

#include "keptech/core/ecs/base.hpp"
#include "keptech/core/maths/transform.hpp"

namespace keptech::components {
  struct Transform {
    maths::Transform local;
    maths::Transform global;

    void recalculateGlobalTransform();

    [[nodiscard]] bool isDirty() const { return dirty; }

    void setParent(const ecs::EntityHandle newParent) {
      if (parent != newParent) {
        parent = newParent;
        dirty = true;
      }
    }

  private:
    bool dirty = false;
    ecs::EntityHandle parent = ecs::INVALID_ENTITY_HANDLE;
  };
} // namespace keptech::components
