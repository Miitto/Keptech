#pragma once

#include "keptech/core/slotmap.hpp"
#include "pipeline.hpp"

namespace keptech::core::rendering {
  struct Material {
    using Handle = SlotMapSmartHandle;

    enum class Stage : uint8_t { Deferred, Forward, Transparent };

    struct CreateInfo {
      Stage stage = Stage::Deferred;
      PipelineCreateInfo pipelineConfig;
    };

    Stage stage;
  };
} // namespace keptech::core::rendering
