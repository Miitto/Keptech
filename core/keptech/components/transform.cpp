#include "keptech/components/transform.hpp"
#include "imgui.h"
#include "keptech/core/gui.h"
#include "keptech/core/profile.hpp"
#include "keptech/core/scene.hpp"

namespace kt::components {
  namespace {
    struct TransformDepthSortState {
      bool needsDepthSort = true;
    };
    struct TransformRecalcState {
      bool needsRecalc = true;
    };
  } // namespace

  void Transform::markDirty() {
    flags.set(Flags::TransformDirty);
    Scene::active().getEcs().ctx().insert_or_assign<TransformRecalcState>(TransformRecalcState{.needsRecalc = true});
  }

  void Transform::markDepthDirty() {
    Scene::active().getEcs().ctx().insert_or_assign<TransformDepthSortState>(TransformDepthSortState{.needsDepthSort = true});
    Scene::active().getEcs().ctx().insert_or_assign<TransformRecalcState>(TransformRecalcState{.needsRecalc = true});
  }

  void Transform::recalculateDepth() {
    KT_PROFILE_FUNCTION
    bool depthChanged = false;
    for (auto& child : children) {
      auto& childTransform = child.getComponents<Transform>();
      if (childTransform.depth != depth + 1) {
        depthChanged = true;
      }
      childTransform.depth = depth + 1;
      childTransform.markDirty();
      childTransform.recalculateDepth();
    }

    if (depthChanged) {
      markDepthDirty();
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

    auto* depthSortState = ecs.ctx().find<TransformDepthSortState>();
    if (!depthSortState || depthSortState->needsDepthSort) {
      ecs.sort<Transform>([](const auto& a, const auto& b) { return a.getDepth() < b.getDepth(); });
      ecs.ctx().insert_or_assign<TransformDepthSortState>(TransformDepthSortState{.needsDepthSort = false});
    }

    auto* recalcState = ecs.ctx().find<TransformRecalcState>();
    if (recalcState && !recalcState->needsRecalc) {
      return;
    }

    auto view = ecs.view<Transform>();
    for (auto [entity, transform] : view.each()) {
      transform.recalculateGlobalTransform();
    }

    ecs.ctx().insert_or_assign<TransformRecalcState>(TransformRecalcState{.needsRecalc = false});
  }

  void Transform::setParent(const ecs::Entity self, const ecs::Entity newParent) {
    if (parent == newParent) {
      return;
    }

    auto oldDepth = depth;

    markDirty();
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

    if (depth != oldDepth) {
      recalculateDepth();
      markDepthDirty();
    }

    recalculateDepth();
  }
  [[nodiscard]] uint32_t Transform::getDepth() const { return depth; }

  [[nodiscard]] const maths::Transform& Transform::getLocal() const { return local; }
  [[nodiscard]] const glm::mat4& Transform::getGlobal() const { return global; }
  maths::Transform& Transform::getLocalMut() {
    markDirty();
    return local;
  }

  [[nodiscard]] ecs::Entity Transform::getParent() const { return parent; }
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

  void Transform::removeChild(const ecs::Entity child) {
    children.erase(std::remove(children.begin(), children.end(), child), children.end());
  }

} // namespace kt::components
