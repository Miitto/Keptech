#pragma once

#include "keptech/core/base.hpp"
#include "keptech/core/layers/layer.hpp"

namespace keptech::core::layers {
  class LayerStack {
    using LayerPtr = std::unique_ptr<Layer>;
    using Vec = std::vector<LayerPtr>;
    using Iter = Vec::iterator;

  public:
    void pushLayer(LayerPtr layer);
    void pushOverlay(LayerPtr overlay);
    void popLayer(Layer* layer);
    void popOverlay(Layer* overlay);

    template <typename L, typename... Args> void emplaceLayer(Args&&... args) {
      auto layer = std::make_unique<L>(std::forward<Args>(args)...);
      layer->onAttach();
      layerInsert = layers.insert(layerInsert, std::move(layer));
    }

    template <typename L, typename... Args>
    void emplaceOverlay(Args&&... args) {
      auto overlay = std::make_unique<L>(std::forward<Args>(args)...);
      overlay->onAttach();
      layers.emplace_back(std::move(overlay));
    }

    void onUpdate(Timestep ts);
    bool onEvent(events::Event& event);

  private:
    Vec layers{};
    Iter layerInsert = layers.begin();
  };
} // namespace keptech::core::layers
