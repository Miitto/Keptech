#include "keptech/core/components/transform.hpp"
#include "imgui.h"
#include "keptech/core/gui.h"

namespace kt::components {

  bool Transform::recalculateGlobalTransform() {
    bool dirty = flags.has(Flags::Dirty);
    flags.clear(Flags::Dirty);

    bool hasParent = parent.isValid();

    Transform* parentTransform = nullptr;
    if (hasParent) {
      parentTransform = &parent.getComponents<Transform>();
      dirty |= parentTransform->recalculateGlobalTransform();
    }

    if (dirty) {
      global = local.toMatrix();

      if (hasParent)
        global = parentTransform->getGlobal() * global;
    }

    return dirty;
  }

  void Transform::inspectorUi(kt::gui::Frame& frame, bool readOnly) {
    frame.separatorText("Transform");

    ImGuiInputTextFlags iflags = !readOnly ? 0 : ImGuiInputTextFlags_ReadOnly;

    glm::vec3 pos = local.pos();
    glm::vec3 rot = glm::degrees(glm::eulerAngles(local.rot()));
    glm::vec3 scale = local.scale();

    if (frame.inputFloat3("Pos:", &pos.x, "%.3f", iflags)) {
      local.setPosition(pos);
      flags.set(Flags::Dirty);
    }
    if (frame.inputFloat3("Rot:", &rot.x, "%.3f", iflags)) {
      local.setRotation(glm::radians(rot));
      flags.set(Flags::Dirty);
    }
    if (frame.inputFloat3("Scl:", &scale.x, "%.3f", iflags)) {
      local.setScale(scale);
      flags.set(Flags::Dirty);
    }
  }
} // namespace kt::components
