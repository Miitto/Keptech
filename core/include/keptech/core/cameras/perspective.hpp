#pragma once

#include "keptech/core/cameras/camera.hpp"

namespace keptech::core::cameras {
  class PerspectiveCamera : public Camera {
  public:
    struct Parameters {
      /// Field of view in the Y direction, in radians.
      float fovY;
      float aspectRatio;
      float nearClip;
      float farClip;
    };

    PerspectiveCamera(const Parameters& params) : params(params) {}

    PerspectiveCamera& setFovY(float fovY) {
      params.fovY = fovY;
      return *this;
    }

    void onViewportResize(int width, int height) override {
      params.aspectRatio =
          static_cast<float>(width) / static_cast<float>(height);
      dirty.set(CameraMatrixFlags::Projection);
    }

  private:
    void remakeProjectionMatrix() override {
      uniforms.projection = glm::perspective(params.fovY, params.aspectRatio,
                                             params.nearClip, params.farClip);
      dirty.clear(CameraMatrixFlags::Projection);
    }

    Parameters params;
  };
} // namespace keptech::core::cameras
