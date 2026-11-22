#include "impls.hpp"

namespace kt::phys {
  JPH::uint BPLayerInterface::GetNumBroadPhaseLayers() const { return BroadPhaseLayers::NUM_LAYERS; }
  JPH::BroadPhaseLayer BPLayerInterface::GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const {
    JPH_ASSERT(inLayer < Layers::NUM_LAYERS);
    return JPH::BroadPhaseLayer(static_cast<JPH::uint8>(inLayer));
  }
  const char* BPLayerInterface::GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const {
    switch (inLayer.GetValue()) {
    case BroadPhaseLayers::eNonMoving:
      return "Non-Moving";
    case BroadPhaseLayers::eMoving:
      return "Moving";
    default:
      JPH_ASSERT(false);
      return "Unknown";
    }
  }

  bool ObjectVsBroadPhaseLayerFilter::ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const {
    return inLayer1 | inLayer2.GetValue();
  }
  void InitializeJolt() {}
} // namespace kt::phys