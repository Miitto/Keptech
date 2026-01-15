#include "keptech/core/components/transform.hpp"
#include "imgui.h"
#include "keptech/core/gui.h"

namespace keptech::components {
  void Transform::recalculateGlobalTransform() {
    if (!dirty) {
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

  void Transform::guiPane(keptech::gui::Frame& frame,
                          core::Bitflag<TransformGuiFlags>& flags) {
    auto child = frame.child("##transform");

    bool isLocal = !flags.has(TransformGuiFlags::GlobalCoords);

    auto& t = isLocal ? local : global;

    ImGuiInputTextFlags iflags = flags.has(TransformGuiFlags::Editable)
                                     ? 0
                                     : ImGuiInputTextFlags_ReadOnly;

    glm::vec3 pos = t.pos();
    glm::vec3 rot = glm::degrees(glm::eulerAngles(t.rot()));
    glm::vec3 scale = t.scale();

    if (child.inputFloat3("Pos:", &pos.x, "%.3f", iflags)) {
      if (isLocal) {
        local.setPosition(pos);
      } else {
        glm::vec3 diff = pos - t.pos();
        local.setPosition(local.pos() + diff);
      }
      dirty = true;
    }
    if (child.inputFloat3("Rot:", &rot.x, "%.3f", iflags)) {
      if (isLocal) {
        local.setRotation(glm::radians(rot));
      } else {
        glm::vec3 curr = glm::eulerAngles(t.rot());
        glm::vec3 diff = glm::radians(rot) - curr;
        local.setRotation(curr + diff);
      }
      dirty = true;
    }
    if (child.inputFloat3("Scl:", &scale.x, "%.3f", iflags)) {
      if (isLocal) {
        local.setScale(scale);
      } else {
        glm::vec3 diff = scale - t.scale();
        local.setScale(local.scale() + diff);
      }
      dirty = true;
    }

    ImGui::Columns(2);
    if (child.selectable("Local", isLocal)) {
      flags.clear(TransformGuiFlags::GlobalCoords);
    }
    ImGui::NextColumn();
    if (child.selectable("Global", !isLocal)) {
      flags.set(TransformGuiFlags::GlobalCoords);
    }
    ImGui::Columns(1);
  }
} // namespace keptech::components
