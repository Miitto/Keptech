#pragma once

#include "keptech/render/interface.hpp"

#ifdef interface
#undef interface
#endif

namespace std {
  template <typename T, typename E> class expected;
}

namespace kt {
  class Window;
  struct RendererCreateInfo;
  namespace maths {
    struct Frustum;
  }
} // namespace kt

namespace kt::rdr {
  struct Formats;
  class RenderGraphBuilder;
  class CommandBuffer;

  namespace interface {
    template <class T>
    concept IsRenderer =
        requires(T t, const T ct, ImageFormat format, RenderGraphBuilder& builder, const RendererCreateInfo& createInfo, Window& window) {
          { T::get() } -> std::same_as<T&>;
          { T::init(createInfo, window) } -> std::same_as<std::expected<void, std::string>>;
          { T::isInit() } -> std::same_as<bool>;

          { t.newFrame() } -> std::same_as<void>;
          { t.startFrame() } -> std::same_as<maths::Frustum>;
          { t.endFrame(std::declval<const CommandBuffer&>()) } -> std::same_as<void>;
          { ct.setRenderGraphProps(builder) } -> std::same_as<void>;

          { t.onResize() } -> std::same_as<void>;

          { ct.canRenderToFormat(format) } -> std::same_as<bool>;
          { ct.getFormats() } -> std::same_as<const Formats&>;
          { t.addGeometryPass(builder, true) } -> std::same_as<void>;
        };
  } // namespace interface
} // namespace kt::rdr