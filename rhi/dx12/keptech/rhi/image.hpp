#pragma once

#include "keptech/rhi/dx/allocation.hpp"
#include "keptech/rhi/imageCreateInfo.hpp"
#include "keptech/rhi/imageFormat.hpp"
#include "keptech/rhi/interface/image_base.hpp"
#include <wrl/client.h>

namespace D3D12MA {
  struct Allocation; // NOLINT
}

namespace kt::rhi {
  class ImageCreateInfo;
  class ImageRef;

  class Image : public ImageBase {

  public:
    friend class ImageBase;

    Microsoft::WRL::ComPtr<ID3D12Resource> dxresource() const;
    uint16_t dxGetRtvDsvIndex() const { return rtvDsvIndex; }
    void dxSetRtvDsvIndex(uint16_t index) { rtvDsvIndex = index; }
    Allocation dxTakeAllocation();

    Image() = default;
    Image(const Image&) = delete;
    Image& operator=(const Image&) = delete;
    Image(Image&& other) noexcept;
    Image& operator=(Image&& other) noexcept;
    ~Image();

  private:
    Image(std::string name, ImageDim dim, ImageFormat format, glm::uvec3 extent, Bitflag<ImageUsage> usage, uint32_t mips, uint32_t layers,
          Allocation allocation)
        : ImageBase(std::move(name), dim, format, extent, usage, mips, layers), allocation(allocation) {}

    Allocation allocation{};
    uint16_t rtvDsvIndex = 65535;
  };
} // namespace kt::rhi