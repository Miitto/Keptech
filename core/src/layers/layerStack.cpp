#include "keptech/core/layers/layerStack.hpp"

namespace keptech::core::layers {
  void LayerStack::pushLayer(LayerPtr layer) {
    layer->onAttach();
    layerInsert = layers.insert(layerInsert, std::move(layer));
  }

  void LayerStack::pushOverlay(LayerPtr overlay) {
    overlay->onAttach();
    layers.emplace_back(std::move(overlay));
  }

  void LayerStack::popLayer(Layer* layer) {
    auto it =
        std::find_if(layers.begin(), layerInsert,
                     [&](const LayerPtr& ptr) { return ptr.get() == layer; });
    if (it != layerInsert) {
      layers.erase(it);
      layerInsert--;
    }
  }

  void LayerStack::popOverlay(Layer* overlay) {
    auto it = std::find_if(layerInsert, layers.end(), [&](const LayerPtr& ptr) {
      return ptr.get() == overlay;
    });
    if (it != layers.end()) {
      layers.erase(it);
    }
  }

  void LayerStack::onUpdate(Timestep ts) {
    for (auto& layer : layers) {
      layer->onUpdate(ts);
    }
  }

  bool LayerStack::onEvent(events::Event& event) {
    for (auto it = layers.rbegin(); it != layers.rend(); ++it) {
      if ((*it)->onEvent(event)) {
        return true;
      }
    }
    return false;
  }
} // namespace keptech::core::layers
