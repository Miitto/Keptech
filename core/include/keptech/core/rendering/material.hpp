#pragma once

#include "keptech/core/slotmap.hpp"
#include "pipeline.hpp"
#include <spdlog/fmt/bundled/format.h>

namespace keptech::core::rendering {
  struct Material {
    struct Handle {
      SlotMapSmartHandle handle;
    };
    struct WeakHandle {
      SlotMapWeakHandle handle;
    };

    enum class Stage : uint8_t { Deferred, Forward, Transparent };

    struct CreateInfo {
      Stage stage = Stage::Deferred;
      PipelineCreateInfo pipelineConfig;
    };

    std::string name;
    Stage stage;
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
    case keptech::core::rendering::Material::Stage::Forward:
      name = "Forward";
      break;
    case keptech::core::rendering::Material::Stage::Transparent:
      name = "Transparent";
      break;
    }
    return fmt::formatter<std::string_view>::format(name, ctx);
  }
};
