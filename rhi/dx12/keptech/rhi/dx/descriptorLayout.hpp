#pragma once

namespace kt::rhi {
  class DescriptorLayout {
  public:
    DescriptorLayout() = default;
    DescriptorLayout(std::vector<D3D12_DESCRIPTOR_RANGE1>&& ranges) : ranges(std::move(ranges)) {}

    [[nodiscard]] auto& dxGetRanges(this auto& self) { return self.ranges; }

  private:
    std::vector<D3D12_DESCRIPTOR_RANGE1> ranges;
  };
} // namespace kt::rhi