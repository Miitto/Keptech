#pragma once

#include "keptech/core/slotmap.hpp"
#include "pipeline.hpp"

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
