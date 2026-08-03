#pragma once

#define MAX_FRAMES_IN_FLIGHT 2

namespace kt::rdr {
  constexpr size_t SWAPCHAIN_IMAGE_COUNT = 3;

  extern size_t RTV_DESCRIPTOR_SIZE; // NOLINT
} // namespace kt::rdr