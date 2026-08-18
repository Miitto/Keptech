#pragma once

#include "keptech/rhi/imageFormat.hpp"
#include <d3d12.h>
#include <wrl/client.h>


namespace kt::rhi {
  class Image;

  class ImageRef {
  public:
    ImageRef() = default;
    ImageRef(const char* name, ID3D12Resource* resource, ImageFormat format, D3D12_CPU_DESCRIPTOR_HANDLE rtvDsvHandle = {},
             uint64_t texIndex = ~0ul)
        : name(name), resource(resource), _format(format), rtvDsvHandle(rtvDsvHandle), texIndex(texIndex) {}
    ImageRef(const ImageRef&) = default;
    ImageRef& operator=(const ImageRef&) = default;

    const char* getName() const { return name; }
    operator ID3D12Resource*() const { return resource; }
    ImageFormat format() const { return _format; }

    bool valid() const { return resource != nullptr; }

    uint64_t getTextureIndex() const { return texIndex; }

    D3D12_CPU_DESCRIPTOR_HANDLE dxGetRtvDsvHandle() const { return rtvDsvHandle; }

    ID3D12Resource* dxGetResource() const { return resource; }

  private:
    const char* name;
    ID3D12Resource* resource;
    ImageFormat _format;
    D3D12_CPU_DESCRIPTOR_HANDLE rtvDsvHandle{};
    uint64_t texIndex = ~0ul;
  };
} // namespace kt::rhi