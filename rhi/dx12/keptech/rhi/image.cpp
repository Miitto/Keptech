#include "image.hpp"
#include "d3dx12.h"
#include "dx/dx-logger.hpp"
#include "imageRef.hpp"
#include "keptech/rhi/imageCreateInfo.hpp"
#include "rhi.hpp"
#include <D3D12MemAlloc.h>

namespace kt::rhi {
  const std::string& Image::getName() const { return name; }
  ImageFormat Image::format() const { return _format; }
  uint32_t Image::mips() const { return _mips; }
  uint32_t Image::layers() const { return _layers; }
  Bitflag<ImageUsage> Image::getUsage() const { return usage; }
  glm::uvec3 Image::getExtent() const { return extent; }
  ImageDim Image::dim() const { return _dim; }

  bool Image::isDepth() const {
    switch (_format) {
    case ImageFormat::D16_UNORM:
    case ImageFormat::D32_FLOAT:
      return true;
    default:
      return false;
    }
  }

  Microsoft::WRL::ComPtr<ID3D12Resource> Image::dxresource() const {
    if (allocation == nullptr) {
      return nullptr;
    }
    return allocation->GetResource();
  }

  kt::Result<Image, HRESULT, 0> Image::create(const ImageCreateInfo& info) {
    DX_ASSERT(info.getExtent().x > 0 && info.getExtent().y > 0 && info.getExtent().z > 0, "Image extent must be greater than 0");
    DX_ASSERT(info.getUsage().intersect(kt::rhi::ImageUsage::RenderTarget | kt::rhi::ImageUsage::DepthStencil) !=
                  (kt::rhi::ImageUsage::RenderTarget | kt::rhi::ImageUsage::DepthStencil),
              "Image cannot be both a render target and a depth stencil");

    CD3DX12_RESOURCE_DESC desc;

    switch (info.getImageDim()) {
    case ImageDim::e1D:
      desc = CD3DX12_RESOURCE_DESC::Tex1D(static_cast<DXGI_FORMAT>(info.getFormat()), info.getWidth(), info.getArrayLayers(),
                                          info.getMipLevels(), raw(info.getUsage().as_enum()));
      break;
    case ImageDim::e2D:
      desc = CD3DX12_RESOURCE_DESC::Tex2D(static_cast<DXGI_FORMAT>(info.getFormat()), info.getWidth(), info.getHeight(),
                                          info.getArrayLayers(), info.getMipLevels(), 1, 0, raw(info.getUsage().as_enum()));
      break;
    case ImageDim::e3D:
      desc = CD3DX12_RESOURCE_DESC::Tex3D(static_cast<DXGI_FORMAT>(info.getFormat()), info.getWidth(), info.getHeight(), info.getDepth(),
                                          info.getMipLevels(), raw(info.getUsage().as_enum()));
      break;
    case ImageDim::eCube:
#ifndef NDEBUG
      if (info.getArrayLayers() != 6) {
        DX_WARN("Creating a cube map with {} array layers, expected 6", info.getArrayLayers());
      }
#endif
      desc = CD3DX12_RESOURCE_DESC::Tex2D(static_cast<DXGI_FORMAT>(info.getFormat()), info.getWidth(), info.getHeight(),
                                          info.getArrayLayers(), info.getMipLevels(), 1, 0, raw(info.getUsage().as_enum()));
    }

    D3D12MA::ALLOCATION_DESC allocDesc{
        .HeapType = D3D12_HEAP_TYPE_DEFAULT,
    };

    D3D12MA::Allocation* allocation = nullptr;
    DX_REQUIRE(SUCCEEDED(RHI::get().dxGetAllocator()->CreateResource(&allocDesc, &desc, D3D12_RESOURCE_STATE_COMMON, nullptr, &allocation,
                                                                     IID_NULL, NULL)),
               "Failed to create image resource");

#ifndef NDEBUG
    std::string debugName = {info.getName()};
    std::wstring wDebugName(debugName.begin(), debugName.end());
    allocation->SetName(wDebugName.c_str());
    allocation->GetResource()->SetPrivateData(WKPDID_D3DDebugObjectName, debugName.length(), debugName.c_str());
#endif

    Image i(info.getName(), info.getImageDim(), info.getFormat(), info.getExtent(), info.getUsage(), info.getMipLevels(),
            info.getArrayLayers(), allocation);

    if (info.getUsage().has(kt::rhi::ImageUsage::RenderTarget)) {
      RHI::get().dxRegisterRenderTargetImage(i);
    } else if (info.getUsage().has(kt::rhi::ImageUsage::DepthStencil)) {
      RHI::get().dxRegisterDepthStencilImage(i);
    }

    return std::move(i);
  }

  kt::Result<Image, HRESULT, S_OK> Image::resize(const glm::uvec3& newExtent) const {
    if (allocation == nullptr) {
      return {E_FAIL};
    }

    auto i = Image::create({_dim, _format, newExtent, usage, _mips, _layers, name.c_str()});
    if (i.isOk()) {
      i.value().dxSetRtvDsvIndex(rtvDsvIndex);
      if (usage.has(kt::rhi::ImageUsage::RenderTarget)) {
        RHI::get().dxUpdateRenderTargetImage(i.value());
      } else if (usage.has(kt::rhi::ImageUsage::DepthStencil)) {
        RHI::get().dxUpdateDepthStencilImage(i.value());
      }
    }
    return i;
  }

  void Image::destroy() {
    if (allocation != nullptr) {
      allocation->Release();
      allocation = nullptr;
    }
  }

  Image::Image(Image&& other) noexcept
      : name(std::move(other.name)), _dim(other._dim), _format(other._format), extent(other.extent), usage(other.usage), _mips(other._mips),
        _layers(other._layers), allocation(other.allocation), rtvDsvIndex(other.rtvDsvIndex) {
    other.allocation = nullptr;
  }

  Image& Image::operator=(Image&& other) noexcept {
    if (this != &other) {
      name = std::move(other.name);
      _dim = other._dim;
      extent = other.extent;
      _format = other._format;
      _mips = other._mips;
      _layers = other._layers;
      allocation = other.allocation;
      rtvDsvIndex = other.rtvDsvIndex;
      usage = other.usage;

      other.allocation = nullptr;
    }
    return *this;
  }

  Image::operator ImageRef() const {

    return ImageRef(name.c_str(), dxresource().Get(), _format,
                    usage.has(kt::rhi::ImageUsage::RenderTarget) ? RHI::get().dxGetRtvHandle(rtvDsvIndex)
                    : usage.has(ImageUsage::DepthStencil)        ? RHI::get().dxGetDsvHandle(rtvDsvIndex)
                                                                 : CD3DX12_CPU_DESCRIPTOR_HANDLE(),
                    textureIndex);
  }

  D3D12MA::Allocation* Image::dxTakeAllocation() {
    auto* alloc = allocation;
    allocation = nullptr;
    return alloc;
  }
} // namespace kt::rhi