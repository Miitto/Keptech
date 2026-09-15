#pragma once

#include "glm/ext/vector_uint2.hpp"
#include "keptech/core/bitflag.hpp"
#include "keptech/core/macros.hpp"
#include "keptech/core/subject.hpp"

namespace kt::rhi {

  enum class SettingsDirty : uint8_t {
    RenderResolution = BIT(0),
  };

  class SettingsEvent {
  public:
    SettingsEvent(SettingsDirty type, const glm::uvec2& renderResolution) : type(type), renderResolution(renderResolution) {}

    SettingsDirty getType() const { return type; }

    glm::uvec2 getRenderResolution() const { return renderResolution; }

  private:
    SettingsDirty type;
    union {
      glm::uvec2 renderResolution;
    };
  };

  class RhiSettings {
  public:
    RhiSettings() = default;

    void setRenderResolution(const glm::uvec2& resolution) {
      if (renderResolution == resolution)
        return;
      renderResolution = resolution;
      dirtyFlags |= SettingsDirty::RenderResolution;
      settingsChangedSubject.notify(SettingsEvent(SettingsDirty::RenderResolution, resolution));
    }

    glm::uvec2 getRenderResolution() const { return renderResolution; }

    const Bitflag<SettingsDirty>& getDirtyFlags() const { return dirtyFlags; }

    Subject<SettingsEvent>& onChanged() { return settingsChangedSubject; }

  private:
    glm::uvec2 renderResolution{};

    Bitflag<SettingsDirty> dirtyFlags;
    Subject<SettingsEvent> settingsChangedSubject;
  };
} // namespace kt::rhi