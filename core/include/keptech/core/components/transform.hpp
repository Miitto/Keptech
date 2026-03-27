#pragma once

#include "keptech/core/bitflag.hpp"
#include "keptech/core/macros.hpp"
#include "keptech/core/maths/transform.hpp"
#include "keptech/ecs/entity.hpp"

namespace kt::gui {
  class Frame;
}

namespace kt::components {
  class Transform {
  public:
    bool recalculateGlobalTransform();

    [[nodiscard]] bool isDirty() const { return flags.has(Flags::Dirty); }

    void setParent(const ecs::Entity newParent) {
      if (parent != newParent) {
        parent = newParent;
        flags.set(Flags::Dirty);
      }
    }

    [[nodiscard]] const maths::Transform& getLocal() const { return local; }
    [[nodiscard]] const glm::mat4& getGlobal() const { return global; }

    maths::Transform& getLocalMut() {
      flags.set(Flags::Dirty);
      return local;
    }

    [[nodiscard]] ecs::Entity getParent() const { return parent; }

    void inspectorUi(kt::gui::Frame& frame, bool readOnly = false);

  private:
    enum class Flags : uint8_t {
      Dirty = BIT(0),
    };

    maths::Transform local;
    glm::mat4 global;
    ecs::Entity parent = ecs::Entity{};
    Bitflag<Flags> flags = Flags::Dirty;
  };
} // namespace kt::components
