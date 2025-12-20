#pragma once

namespace keptech::core::renderer {
  template <typename T>
  concept Renderer = requires(T a) {
    typename T::Mesh;
    typename T::Material;
    typename T::RenderObject;

    { a.newFrame() } -> std::same_as<void>;
    { a.render() } -> std::same_as<void>;
  };
} // namespace keptech::core::renderer
