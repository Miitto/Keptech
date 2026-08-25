#pragma once

#include "keptech/rhi/imageCreateInfo.hpp"
#include "keptech/rhi/imageFormat.hpp"
#include <d3d12.h>
#include <wrl/client.h>

namespace kt::rhi {
  class Image;

  class ImageRef {
  public:
    ImageRef() = default;
    ImageRef(const char* name, ID3D12Resource* resource, ImageDim dimension, ImageFormat format, uint32_t mips, uint32_t layers,
             D3D12_CPU_DESCRIPTOR_HANDLE rtvDsvHandle = {}, uint64_t texIndex = ~0ul)
        : name(name), resource(resource), dimension(dimension), _format(format), _mips(mips), _layers(layers), rtvDsvHandle(rtvDsvHandle),
          texIndex(texIndex) {}
    ImageRef(const ImageRef&) = default;
    ImageRef& operator=(const ImageRef&) = default;

    const char* getName() const { return name; }
    operator ID3D12Resource*() const { return resource; }
    ImageFormat format() const { return _format; }
    ImageDim dim() const { return dimension; }
    uint32_t mips() const { return _mips; }
    uint32_t layers() const { return _layers; }

    bool valid() const { return resource != nullptr; }

    uint64_t getTextureIndex() const { return texIndex; }

    D3D12_CPU_DESCRIPTOR_HANDLE dxGetRtvDsvHandle() const { return rtvDsvHandle; }

    ID3D12Resource* dxGetResource() const { return resource; }

  private:
    const char* name;
    ID3D12Resource* resource;
    ImageDim dimension;
    ImageFormat _format;
    uint32_t _mips;
    uint32_t _layers;
    D3D12_CPU_DESCRIPTOR_HANDLE rtvDsvHandle{};
    uint64_t texIndex = ~0ul;
  };
} // namespace kt::rhi