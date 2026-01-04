#pragma once

#include "keptech/core/maths/transform.hpp"
#include "keptech/ecs/entity.hpp"

namespace keptech::components {
  class Transform {
  public:
    void recalculateGlobalTransform();

    [[nodiscard]] bool isDirty() const { return dirty; }

    void setParent(const ecs::Entity newParent) {
      if (parent != newParent) {
        parent = newParent;
        dirty = true;
      }
    }

    [[nodiscard]] const maths::Transform& getLocal() const { return local; }
    [[nodiscard]] const maths::Transform& getGlobal() const { return global; }

    maths::Transform& getLocalMut() {
      dirty = true;
      return local;
    }

  private:
    maths::Transform local;
    maths::Transform global;
    bool dirty = false;
    ecs::Entity parent = ecs::Entity{};
  };
} // namespace keptech::components
