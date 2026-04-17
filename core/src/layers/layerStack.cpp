#include "keptech/core/layers/layerStack.hpp"
#include "keptech/core/profile.hpp"
#include <algorithm>
#include <ranges>

namespace kt::core::layers {
  void LayerStack::pushLayer(LayerPtr layer) {
    layer->onAttach();
    layerInsert = layers.insert(layerInsert, std::move(layer));
  }

  void LayerStack::pushOverlay(LayerPtr overlay) {
    overlay->onAttach();
    layers.emplace_back(std::move(overlay));
  }

  void LayerStack::popLayer(Layer* layer) {
    auto it = std::find_if(layers.begin(), layerInsert, [&](const LayerPtr& ptr) { return ptr.get() == layer; });
    if (it != layerInsert) {
      layers.erase(it);
      layerInsert--;
    }
  }

  void LayerStack::popOverlay(Layer* overlay) {
    auto it = std::find_if(layerInsert, layers.end(), [&](const LayerPtr& ptr) { return ptr.get() == overlay; });
    if (it != layers.end()) {
      layers.erase(it);
    }
  }

  void LayerStack::onUpdate(Timestep ts) {
    KT_PROFILE_FUNCTION
    for (auto& layer : layers) {
      layer->onUpdate(ts);
    }
  }

  bool LayerStack::onEvent(events::Event& event, Timestep ts) {
    KT_PROFILE_FUNCTION
    for (auto& layer : std::ranges::reverse_view(layers)) {
      layer->onEvent(event, ts);
      if (event.isHandled())
        return true;
    }
    return false;
  }
} // namespace kt::core::layers
