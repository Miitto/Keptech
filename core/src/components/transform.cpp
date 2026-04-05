#include "keptech/components/transform.hpp"
#include "imgui.h"
#include "keptech/core/gui.h"

namespace kt::components {

  void Transform::recalculateDepth() {
    if (parent.isValid()) {
      auto& parentTransform = parent.getComponents<Transform>();
      parentTransform.recalculateDepth();
      depth = parentTransform.depth + 1;
    }
  }

  void Transform::recalculateGlobalTransform() {
    global = local.toMatrix();

    if (parent.isValid()) {
      auto& parentTransform = parent.getComponents<Transform>();
      global = parentTransform.getGlobal() * global;
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
