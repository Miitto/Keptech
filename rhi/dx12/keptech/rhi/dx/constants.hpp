#pragma once

#define MAX_FRAMES_IN_FLIGHT 2

namespace kt::rhi {
  constexpr size_t SWAPCHAIN_IMAGE_COUNT = 3;

  extern size_t RTV_DESCRIPTOR_SIZE;         // NOLINT
  extern size_t DSV_DESCRIPTOR_SIZE;         // NOLINT
  extern size_t CBV_SRV_UAV_DESCRIPTOR_SIZE; // NOLINT

  extern D3D_ROOT_SIGNATURE_VERSION ROOT_SIGNATURE_VERSION; // NOLINT
} // namespace kt::rhi