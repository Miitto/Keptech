#pragma once

#ifdef near
#undef near
#endif

#ifdef far
#undef far
#endif

#include "keptech/maths/frustum.hpp"
#include "keptech/maths/sizes.hpp"

namespace kt::components {
  enum class ProjectionType : uint8_t {
    Orthographic,
    Perspective,
    PerspectiveInfinite,
  };

  enum class PerspectiveType : uint8_t {
    Standard = static_cast<uint8_t>(ProjectionType::Perspective),
    Infinite = static_cast<uint8_t>(ProjectionType::PerspectiveInfinite),
  };

  class Camera {
  public:
    struct Uniforms {
      glm::mat4 projectionMatrix;
      glm::mat4 viewMatrix;
      glm::mat4 viewProjectionMatrix;

      glm::mat4 invProjectionMatrix;
      glm::mat4 invViewMatrix;
      glm::mat4 invViewProjectionMatrix;
      maths::Frustum frustum;
      glm::vec2 viewportSize;
    };

    struct Params {
      struct Planes {
        float near{0.1f};
        float far{1000.0f};
      };

      struct Common {
        float aspectRatio{16.0f / 9.0f};
        Planes planes;
      } common;

      struct Perspective {
        float fovY{glm::radians(60.0f)};
      };

      struct Orthographic {
        float zoom = 1.0f;
      };
      union {
        Perspective perspective;
        Orthographic orthographic;
      };
    };

    Camera(PerspectiveType perspectiveType, Params::Common common, Params::Perspective perspective, maths::Viewport viewport = {},
           maths::Rect2D scissor = {})
        : projectionType(static_cast<ProjectionType>(perspectiveType)), viewport(viewport), scissor(scissor),
          params({.common = common, .perspective = perspective}) {}

    void recalculateProjectionMatrix();

    void sizeToWindowSize(glm::ivec2 size, bool correctAspectRatio = true) {
      if (correctAspectRatio)
        params.common.aspectRatio = static_cast<float>(size.x) / static_cast<float>(size.y);
      viewport.width = static_cast<float>(size.x);
      viewport.height = static_cast<float>(size.y);
      scissor.width = static_cast<uint32_t>(size.x);
      scissor.height = static_cast<uint32_t>(size.y);
      dirty = true;
    }

    glm::mat4& getProjectionMatrix() { return projectionMatrix; }

    [[nodiscard]] ProjectionType getProjectionType() const { return projectionType; }

    [[nodiscard]] bool isPerspective() const {
      return projectionType == ProjectionType::Perspective || projectionType == ProjectionType::PerspectiveInfinite;
    }
    [[nodiscard]] bool isOrthographic() const { return projectionType == ProjectionType::Orthographic; }

    [[nodiscard]] const Params& getParams() const { return params; }

    Camera& setCommonParams(Params::Common common) {
      dirty = true;
      params.common = common;
      return *this;
    }

    Camera& setOrthographic(Params::Orthographic orthographic) {
      dirty = true;
      projectionType = ProjectionType::Orthographic;
      params.orthographic = orthographic;
      return *this;
    }

    Camera& setPerspective(PerspectiveType perspectiveType, Params::Perspective perspective) {
      dirty = true;
      projectionType = static_cast<ProjectionType>(perspectiveType);
      params.perspective = perspective;
      return *this;
    }

    maths::Viewport& getViewport() { return viewport; }
    maths::Rect2D& getScissor() { return scissor; }
    [[nodiscard]] const maths::Viewport& getViewport() const { return viewport; }
    [[nodiscard]] const maths::Rect2D& getScissor() const { return scissor; }

  private:
    glm::mat4 projectionMatrix{1.0f};
    ProjectionType projectionType{ProjectionType::Perspective};

    maths::Viewport viewport{};
    maths::Rect2D scissor{};

    Params params{};
    bool dirty = true;
  };
} // namespace kt::components
