#pragma once

#include "keptech/std/vec.hpp"
#include <d3d12.h>

namespace kt::rhi {
  class DescriptorLayout {
  public:
    DescriptorLayout() = default;
    DescriptorLayout(Vec<D3D12_DESCRIPTOR_RANGE1>&& ranges) : ranges(std::move(ranges)) {}

    [[nodiscard]] auto& dxGetRanges(this auto& self) { return self.ranges; }

    operator bool() const { return !ranges.empty(); }

  private:
    Vec<D3D12_DESCRIPTOR_RANGE1> ranges;
  };
} // namespace kt::rhi