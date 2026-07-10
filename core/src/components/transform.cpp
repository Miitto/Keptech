#include "keptech/components/transform.hpp"
#include "imgui.h"
#include "keptech/core/gui.h"
#include "keptech/core/profile.hpp"

namespace kt::components {

  void Transform::recalculateDepth() {
    KT_PROFILE_FUNCTION
    for (auto& child : children) {
      auto& childTransform = child.getComponents<Transform>();
      childTransform.depth = depth + 1;
      childTransform.flags.set(Flags::TransformDirty);
      childTransform.recalculateDepth();
    }
  }

  void Transform::recalculateGlobalTransform() {
    KT_PROFILE_FUNCTION
    if (flags.has(Flags::TransformDirty)) {
      flags.clear(Flags::TransformDirty);
      global = local.toMatrix();

      for (auto& child : children) {
        auto& childTransform = child.getComponents<Transform>();
        childTransform.flags.set(Flags::TransformDirty);
      }

      if (parent.isValid()) {
        auto& parentTransform = parent.getComponents<Transform>();
        global = parentTransform.getGlobal() * global;
      }
    }
  }

  void Transform::recalcAllTransforms(ecs::Ecs& ecs) {
    KT_PROFILE_FUNCTION
    auto view = ecs.view<Transform>();
    for (auto [entity, transform] : view.each()) {
      transform.recalculateGlobalTransform();
    }
  }

  void Transform::setParent(const ecs::Entity self, const ecs::Entity newParent) {
    if (parent != newParent) {
      flags.set(Flags::TransformDirty);
      if (parent.isValid() && parent.hasAllComponents<Transform>()) {
        auto& p = parent.getComponents<Transform>();
        p.removeChild(self);
        depth = p.depth + 1;
      } else {
        depth = 1;
      }
      parent = newParent;
      if (newParent.isValid() && newParent.hasAllComponents<Transform>()) {
        auto& p = newParent.getComponents<Transform>();
        p.children.push_back(self);
      }
      recalculateDepth();
    }
  }

  void Transform::inspectorUi(kt::gui::Frame& frame, bool readOnly) {
    frame.separatorText("Transform");

    ImGuiInputTextFlags iflags = !readOnly ? 0 : ImGuiInputTextFlags_ReadOnly;

    glm::vec3 pos = local.pos();
    glm::vec3 rot = glm::degrees(glm::eulerAngles(local.rot()));
    glm::vec3 scale = local.scale();

    if (frame.inputFloat3("Pos:", &pos.x, "%.3f", iflags)) {
      local.setPosition(pos);
    }
    if (frame.inputFloat3("Rot:", &rot.x, "%.3f", iflags)) {
      local.setRotation(glm::radians(rot));
    }
    if (frame.inputFloat3("Scl:", &scale.x, "%.3f", iflags)) {
      local.setScale(scale);
    }
  }
} // namespace kt::components
