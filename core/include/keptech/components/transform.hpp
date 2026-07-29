#pragma once

#include "keptech/core/bitflag.hpp"
#include "keptech/core/macros.hpp"
#include "keptech/ecs/entity.hpp"
#include "keptech/maths/transform.hpp"

namespace kt::gui {
  class Frame;
}

namespace kt {
  class Scene;
}

namespace kt::components {
  class Transform {
  public:
    enum class Flags : uint8_t {
      None = 0,
      TransformDirty = BIT(0),
    };

    void recalculateDepth();
    void recalculateGlobalTransform();

    static void recalcAllTransforms(ecs::Ecs& ecs);

    void setParent(const ecs::Entity self, const ecs::Entity newParent);

    [[nodiscard]] uint32_t getDepth() const;
    [[nodiscard]] const maths::Transform& getLocal() const;
    [[nodiscard]] const glm::mat4& getGlobal() const;

    maths::Transform& getLocalMut();

    [[nodiscard]] ecs::Entity getParent() const;

    void inspectorUi(kt::gui::Frame& frame, bool readOnly = false);

    void markDirty();
    void markDepthDirty();

  private:
    void removeChild(const ecs::Entity child);

    maths::Transform local;
    glm::mat4 global;
    ecs::Entity parent = ecs::Entity{};
    std::vector<ecs::Entity> children{};
    uint32_t depth = 0;
    Bitflag<Flags> flags = Flags::TransformDirty;
  };
} // namespace kt::components
