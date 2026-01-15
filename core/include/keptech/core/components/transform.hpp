#pragma once

#include "keptech/core/bitflag.hpp"
#include "keptech/core/macros.hpp"
#include "keptech/core/maths/transform.hpp"
#include "keptech/ecs/entity.hpp"

namespace keptech::gui {
  class Frame;
}

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

    enum class TransformGuiFlags : uint8_t {
      Editable = BIT(0),
      GlobalCoords = BIT(1),
    };
    void guiPane(keptech::gui::Frame& frame,
                 core::Bitflag<TransformGuiFlags>& flags);

  private:
    maths::Transform local;
    maths::Transform global;
    bool dirty = false;
    ecs::Entity parent = ecs::Entity{};
  };
} // namespace keptech::components
