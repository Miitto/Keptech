#include "image.hpp"
#include "d3dx12.h"
#include "dx-logger.hpp"
#include "renderer.hpp"
#include "wrappers/imageCreateInfo.hpp"
#include <D3D12MemAlloc.h>

namespace kt::rdr {
  const std::string& Image::getName() const { return name; }
  ImageFormat Image::format() const { return _format; }
  uint32_t Image::mips() const { return _mips; }
  uint32_t Image::layers() const { return _layers; }

  kt::Result<Image, HRESULT, 0> Image::create(const ImageCreateInfo& info) {
    CD3DX12_RESOURCE_DESC desc;

    switch (info.getImageDim()) {
    case ImageDim::e1D:
      desc = CD3DX12_RESOURCE_DESC::Tex1D(static_cast<DXGI_FORMAT>(info.getFormat()), info.getWidth(), info.getArrayLayers(),
                                          info.getMipLevels());
      break;
    case ImageDim::e2D:
      desc = CD3DX12_RESOURCE_DESC::Tex2D(static_cast<DXGI_FORMAT>(info.getFormat()), info.getWidth(), info.getHeight(),
                                          info.getArrayLayers(), info.getMipLevels());
      break;
    case ImageDim::e3D:
      desc = CD3DX12_RESOURCE_DESC::Tex3D(static_cast<DXGI_FORMAT>(info.getFormat()), info.getWidth(), info.getHeight(), info.getDepth(),
                                          info.getMipLevels());
      break;
    }

    D3D12MA::ALLOCATION_DESC allocDesc{
        .HeapType = D3D12_HEAP_TYPE_DEFAULT,
    };

    D3D12MA::Allocation* allocation = nullptr;
    DX_REQUIRE(SUCCEEDED(Renderer::get().getMembers().allocator->CreateResource(&allocDesc, &desc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                                                                                &allocation, IID_NULL, NULL)),
               "Failed to create image resource");

    return Image(info.getName(), info.getFormat(), info.getMipLevels(), info.getArrayLayers(), allocation);
  }

  void Image::destroy() {
    if (allocation != nullptr) {
      allocation->Release();
      allocation = nullptr;
    }
  }

  Image::Image(Image&& other) noexcept
      : name(std::move(other.name)), _format(other._format), _mips(other._mips), _layers(other._layers), allocation(other.allocation) {
    other.allocation = nullptr;
  }

  Image& Image::operator=(Image&& other) noexcept {
    if (this != &other) {
      name = std::move(other.name);
      _format = other._format;
      _mips = other._mips;
      _layers = other._layers;
      allocation = other.allocation;

      other.allocation = nullptr;
    }
    return *this;
  }
} // namespace kt::rdr