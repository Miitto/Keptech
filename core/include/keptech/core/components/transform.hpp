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

    [[nodiscard]] bool isDirty() const { return flags.has(Flags::Dirty); }

    void setParent(const ecs::Entity newParent) {
      if (parent != newParent) {
        parent = newParent;
        flags.set(Flags::Dirty);
      }
    }

    [[nodiscard]] const maths::Transform& getLocal() const { return local; }
    [[nodiscard]] const maths::Transform& getGlobal() const { return global; }

    maths::Transform& getLocalMut() {
      flags.set(Flags::Dirty);
      return local;
    }

    [[nodiscard]] ecs::Entity getParent() const { return parent; }

    void inspectorUi(keptech::gui::Frame& frame, bool readOnly = false);

  private:
    enum class Flags : uint8_t {
      Dirty = BIT(0),
      GlobalCoords = BIT(1),
    };

    maths::Transform local;
    maths::Transform global;
    ecs::Entity parent = ecs::Entity{};
    core::Bitflag<Flags> flags = Flags::Dirty;
  };
} // namespace keptech::components
