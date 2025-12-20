#pragma once

#include <keptech/core/maths/transform.hpp>
#include <keptech/core/renderer.hpp>

namespace keptech {
  template <typename R>
    requires keptech::core::renderer::Renderer<R>
  class Object {
  public:
    struct Transforms {
      maths::Transform local;
      maths::Transform world;
    };

    Object(std::string name) : name(std::move(name)) {}

    Object& addChild(Object&& child) {
      children.emplace_back(std::move(child));
      children.back().setParent(this);
      children.back().invalidateWorldTransform();
      return *this;
    }

    [[nodiscard]] Object* getParent() const { return parent; }

    Object& setParent(Object* newParent) {
      parent = newParent;
      return *this;
    }

    void invalidateWorldTransform() {
      if (parent) {
        transform.world = parent->getTransforms().world;
        transform.world.apply(transform.local);
      } else {
        transform.world = transform.local;
      }

      for (auto& child : children) {
        child.invalidateWorldTransform();
      }
    }

    [[nodiscard]] const std::string& getName() const { return name; }

    [[nodiscard]] Transforms& getTransforms() { return transform; }
    [[nodiscard]] const Transforms& getTransforms() const { return transform; }

    Object& makeRenderable(R::RenderObject renderObj) {
      renderObject = std::move(renderObj);
      if (renderObject) {
        renderObject->transform = &transform.world;
      }
      return *this;
    }

    [[nodiscard]] bool isRenderable() const { return renderObject.has_value(); }
    [[nodiscard]] std::optional<typename R::RenderObject>& getRenderObject() {
      return renderObject;
    }

  private:
    Object* parent = nullptr;
    std::vector<Object> children;

    std::string name;
    Transforms transform;

    std::optional<typename R::RenderObject> renderObject;
  };
} // namespace keptech
