#include "keptech/core/components/transform.hpp"

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
} // namespace keptech::components
