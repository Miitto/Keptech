#pragma once

#include <concepts>
#include <cstdint>
#include <glm/fwd.hpp>

namespace kt {
  enum class ImageLayout : uint8_t {
    Undefined,
    RenderTarget,
    ShaderReadOnly,
    TransferSrc,
    TransferDst,
    ComputeReadWrite,
    Present,
  };

  enum class ImageType : uint8_t {
    Color,
    Depth,
    Stencil,
    DepthStencil,
  };

  enum class ImageDimension : uint8_t {
    e1D,
    e2D,
    e3D,
  };
  namespace interface {

    template <typename T>
    concept TransitionInfo = requires(T t) {
      { T(ImageType::Color, ImageLayout::Undefined, ImageLayout::RenderTarget, 1, 1) } -> std::same_as<T>;
    };

    template <typename T>
    concept Image = requires(T t) {
      { std::remove_const_t<decltype(t.extent())>() } -> std::same_as<glm::uvec3>;
      { t.type() } -> std::same_as<typename T::Type>;
      { t.format() } -> std::same_as<typename T::Format>;
    } && TransitionInfo<typename T::TransitionInfoType>;
  } // namespace interface
} // namespace kt