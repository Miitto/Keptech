#pragma once

#include "keptech/ecs/entity.hpp"
#include "keptech/maths/transform.hpp"

namespace kt::gui {
  class Frame;
}

namespace kt::components {
  class Transform {
  public:
    void recalculateDepth();
    void recalculateGlobalTransform();

    void setParent(const ecs::Entity newParent) {
      if (parent != newParent) {
        parent = newParent;
        recalculateDepth();
      }
    }

    [[nodiscard]] uint32_t getDepth() const { return depth; }
    [[nodiscard]] const maths::Transform& getLocal() const { return local; }
    [[nodiscard]] const glm::mat4& getGlobal() const { return global; }

    maths::Transform& getLocalMut() { return local; }

    [[nodiscard]] ecs::Entity getParent() const { return parent; }

    void inspectorUi(kt::gui::Frame& frame, bool readOnly = false);

  private:
    maths::Transform local;
    glm::mat4 global;
    ecs::Entity parent = ecs::Entity{};
    uint32_t depth = 0;
  };
} // namespace kt::components
