#pragma once

#include "keptech/rhi/imageFormat.hpp"
#include <wrl/client.h>

namespace kt::rhi {
  class Image;

  class ImageRef {
  public:
    ImageRef() = default;
    ImageRef(const char* name, ID3D12Resource* resource, ImageFormat format, D3D12_CPU_DESCRIPTOR_HANDLE rtvDsvHandle = {})
        : name(name), resource(resource), _format(format), rtvDsvHandle(rtvDsvHandle) {}

    const char* getName() const { return name; }
    operator ID3D12Resource*() const { return resource; }
    ImageFormat format() const { return _format; }
    D3D12_CPU_DESCRIPTOR_HANDLE dxGetRtvDsvHandle() const { return rtvDsvHandle; }

    ID3D12Resource* dxGetResource() const { return resource; }

  private:
    const char* name;
    ID3D12Resource* resource;
    ImageFormat _format;
    D3D12_CPU_DESCRIPTOR_HANDLE rtvDsvHandle{};
  };
} // namespace kt::rhi