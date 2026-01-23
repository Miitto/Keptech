#pragma once

#include "keptech/core/slotmap.hpp"
#include "pipeline.hpp"
#include <spdlog/fmt/bundled/format.h>

namespace keptech::core::rendering {
  namespace _priv {
    struct MaterialHandleDifferentiator {};
  } // namespace _priv

  struct Material {
    using Handle = core::SlotMapHandle<_priv::MaterialHandleDifferentiator>;
    using SmartHandle =
        core::SlotMapSmartHandle<_priv::MaterialHandleDifferentiator>;

    enum class Stage : uint8_t { Deferred, Opaque, Transparent };

    struct CreateInfo {
      const keptech::shaders::Shader& shader; // NOLINT
      PipelineCreateInfo pipelineConfig{};
    };

    std::string name;
    Stage stage;
    shaders::RenderingMode mode;
  };
} // namespace keptech::core::rendering

template <>
struct fmt::formatter<keptech::core::rendering::Material::Stage>
    : fmt::formatter<std::string_view> {
  template <typename FormatContext>
  auto format(const keptech::core::rendering::Material::Stage stage,
              FormatContext& ctx) const {
    std::string_view name = "";
    switch (stage) {
    case keptech::core::rendering::Material::Stage::Deferred:
      name = "Deferred";
      break;
    case keptech::core::rendering::Material::Stage::Opaque:
      name = "Opaque";
      break;
    case keptech::core::rendering::Material::Stage::Transparent:
      name = "Transparent";
      break;
    }
    return fmt::formatter<std::string_view>::format(name, ctx);
  }
};
