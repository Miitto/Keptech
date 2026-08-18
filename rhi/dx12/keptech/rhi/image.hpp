#pragma once

#include "keptech/core/result.hpp"
#include "keptech/rhi/imageCreateInfo.hpp"
#include "keptech/rhi/imageFormat.hpp"
#include <wrl/client.h>

namespace D3D12MA {
  struct Allocation;
}

namespace kt::rhi {
  class ImageCreateInfo;
  class ImageRef;

  class Image {
#include "keptech/rhi/interface/image.hpp"

  public:
    Microsoft::WRL::ComPtr<ID3D12Resource> dxresource() const;
    uint16_t dxGetRtvDsvIndex() const { return rtvDsvIndex; }
    void dxSetRtvDsvIndex(uint16_t index) { rtvDsvIndex = index; }
    D3D12MA::Allocation* dxTakeAllocation();

  private:
    Image(std::string name, ImageDim dim, ImageFormat format, glm::uvec3 extent, Bitflag<ImageUsage> usage, uint32_t mips, uint32_t layers,
          D3D12MA::Allocation* allocation)
        : name(std::move(name)), _dim(dim), _format(format), extent(extent), usage(usage), _mips(mips), _layers(layers),
          allocation(allocation) {}

    std::string name;
    ImageDim _dim;
    ImageFormat _format;
    glm::uvec3 extent;
    Bitflag<ImageUsage> usage;
    uint32_t _mips;
    uint32_t _layers;
    D3D12MA::Allocation* allocation = nullptr;
    uint16_t rtvDsvIndex = 65535;
    uint64_t textureIndex = UINT64_MAX;
  };
} // namespace kt::rhi