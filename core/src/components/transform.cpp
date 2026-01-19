#include "keptech/core/components/transform.hpp"
#include "imgui.h"
#include "keptech/core/gui.h"

namespace keptech::components {
  void Transform::recalculateGlobalTransform() {
    if (!flags.has(Flags::Dirty)) {
      return;
    }

    bool hasParent = parent.isValid();

    Transform* parentTransform = nullptr;
    if (hasParent) {
      parentTransform = &parent.getComponents<Transform>();
      parentTransform->recalculateGlobalTransform();
    }

    global = local;

    if (hasParent)
      global.apply(parentTransform->global);
  }

  void Transform::inspectorUi(keptech::gui::Frame& frame, bool readOnly) {
    frame.separatorText("Transform");

    bool isLocal = !flags.has(Flags::GlobalCoords);

    auto& t = isLocal ? local : global;

    ImGuiInputTextFlags iflags = !readOnly ? 0 : ImGuiInputTextFlags_ReadOnly;

    glm::vec3 pos = t.pos();
    glm::vec3 rot = glm::degrees(glm::eulerAngles(t.rot()));
    glm::vec3 scale = t.scale();

    if (frame.inputFloat3("Pos:", &pos.x, "%.3f", iflags)) {
      if (isLocal) {
        local.setPosition(pos);
      } else {
        glm::vec3 diff = pos - t.pos();
        local.setPosition(local.pos() + diff);
      }
      flags.set(Flags::Dirty);
    }
    if (frame.inputFloat3("Rot:", &rot.x, "%.3f", iflags)) {
      if (isLocal) {
        local.setRotation(glm::radians(rot));
      } else {
        glm::vec3 curr = glm::eulerAngles(t.rot());
        glm::vec3 diff = glm::radians(rot) - curr;
        local.setRotation(curr + diff);
      }
      flags.set(Flags::Dirty);
    }
    if (frame.inputFloat3("Scl:", &scale.x, "%.3f", iflags)) {
      if (isLocal) {
        local.setScale(scale);
      } else {
        glm::vec3 diff = scale - t.scale();
        local.setScale(local.scale() + diff);
      }
      flags.set(Flags::Dirty);
    }

    ImGui::Columns(2);
    if (frame.selectable("Local", isLocal)) {
      flags.clear(Flags::GlobalCoords);
    }
    ImGui::NextColumn();
    if (frame.selectable("Global", !isLocal)) {
      flags.set(Flags::GlobalCoords);
    }
    ImGui::Columns(1);
  }
} // namespace keptech::components
