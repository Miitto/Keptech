#include "image.hpp"
#include "d3dx12.h"
#include "dx/dx-logger.hpp"
#include "imageRef.hpp"
#include "keptech/maths/maths.hpp"
#include "keptech/rhi/imageCreateInfo.hpp"
#include "rhi.hpp"
#include <D3D12MemAlloc.h>

namespace kt::rhi {
  void ImageBase::setName(this Image& self, const std::string& newName) {
    self.name = newName;
    std::string debugName = fmt::format("Image {}", self.name);
    std::wstring wDebugName(debugName.begin(), debugName.end());
    self.allocation->SetName(wDebugName.c_str());
    self.allocation->GetResource()->SetName(wDebugName.c_str());
  }

  bool ImageBase::operator==(this const Image& self, const Image& other) { return self.allocation == other.allocation; }

  bool ImageBase::isDepth(this const Image& self) {
    switch (self._format) {
    case ImageFormat::D16_UNORM:
    case ImageFormat::D32_FLOAT:
      return true;
    default:
      return false;
    }
  }

  Microsoft::WRL::ComPtr<ID3D12Resource> Image::dxresource() const { return allocation ? allocation->GetResource() : nullptr; }

  kt::Result<Image, HRESULT, 0> ImageBase::create(const ImageCreateInfo& info) {
    DX_ASSERT(info.getExtent().x > 0 && info.getExtent().y > 0 && info.getExtent().z > 0, "Image extent must be greater than 0");
    DX_ASSERT(info.getUsage().intersect(kt::rhi::ImageUsage::RenderTarget | kt::rhi::ImageUsage::DepthStencil) !=
                  (kt::rhi::ImageUsage::RenderTarget | kt::rhi::ImageUsage::DepthStencil),
              "Image cannot be both a render target and a depth stencil");

    CD3DX12_RESOURCE_DESC desc;

    auto format = raw(info.getFormat());
    bool isSampled = info.getUsage().has(kt::rhi::ImageUsage::Sampled);
    if (isSampled) {
      if (format == DXGI_FORMAT_D32_FLOAT)
        format = DXGI_FORMAT_R32_TYPELESS;
      if (format == DXGI_FORMAT_D16_UNORM)
        format = DXGI_FORMAT_R16_TYPELESS;
    }

    uint16_t mipLevels = static_cast<uint16_t>(info.getMipLevels());
    if (mipLevels == 0) {
      mipLevels = static_cast<uint16_t>(kt::calcMipLevels(info.getExtent()));
    }

    switch (info.getImageDim()) {
    case ImageDim::e1D:
      desc = CD3DX12_RESOURCE_DESC::Tex1D(format, info.getWidth(), static_cast<uint16_t>(info.getArrayLayers()), mipLevels,
                                          raw(info.getUsage().as_enum()));
      break;
    case ImageDim::e2D:
      desc = CD3DX12_RESOURCE_DESC::Tex2D(format, info.getWidth(), info.getHeight(), static_cast<uint16_t>(info.getArrayLayers()),
                                          mipLevels, 1, 0, raw(info.getUsage().as_enum()));
      break;
    case ImageDim::e3D:
      desc = CD3DX12_RESOURCE_DESC::Tex3D(format, info.getWidth(), info.getHeight(), static_cast<uint16_t>(info.getDepth()), mipLevels,
                                          raw(info.getUsage().as_enum()));
      break;
    case ImageDim::eCube:
#ifndef NDEBUG
      if (info.getArrayLayers() % 6 != 0) {
        DX_ERROR("Creating a cube map with {} array layers, which is not a multiple of 6. This will cause problems.",
                 info.getArrayLayers());
      }
#endif
      desc = CD3DX12_RESOURCE_DESC::Tex2D(format, info.getWidth(), info.getHeight(), static_cast<uint16_t>(info.getArrayLayers()),
                                          mipLevels, 1, 0, raw(info.getUsage().as_enum()));
    }

    D3D12MA::ALLOCATION_DESC allocDesc{
        .HeapType = D3D12_HEAP_TYPE_DEFAULT,
    };

    D3D12MA::Allocation* allocation = nullptr;
    DX_REQUIRE(SUCCEEDED(RHI::get().dxGetAllocator()->CreateResource(&allocDesc, &desc, D3D12_RESOURCE_STATE_COMMON, nullptr, &allocation,
                                                                     IID_NULL, NULL)),
               "Failed to create image resource");

    Image i(info.getName(), info.getImageDim(), info.getFormat(), info.getExtent(), info.getUsage(), info.getMipLevels(),
            info.getArrayLayers(), Allocation::unique(allocation));

    i.setName(info.getName());

    if (info.getUsage().has(kt::rhi::ImageUsage::RenderTarget)) {
      RHI::get().dxRegisterRenderTargetImage(i);
    } else if (info.getUsage().has(kt::rhi::ImageUsage::DepthStencil)) {
      RHI::get().dxRegisterDepthStencilImage(i);
    }

    if (info.getUsage().has(kt::rhi::ImageUsage::Sampled)) {
      RHI::get().dxRegisterSampledImage(i);
    }

#ifndef KT_DISABLE_STATS
    auto& stats = RHI::get().getStats();
    if (info.getFlags().has(ImageCreateFlags::EngineImage)) {
      ++stats.engineImageCount;
      stats.engineImageMemUsage += allocation->GetSize();
    } else {
      ++stats.userImageCount;
      stats.userImageMemUsage += allocation->GetSize();
    }
#endif

    return std::move(i);
  }

  kt::Result<Image, HRESULT, S_OK> ImageBase::resize(this const Image& self, const glm::uvec3& newExtent) {
    if (!self.allocation) {
      return {E_FAIL};
    }

    auto i = ImageBase::create({self._dim, self._format, newExtent, self.usage, self._mips, self._layers, self.name.c_str()});
    if (i.isOk()) {
      i.value().dxSetRtvDsvIndex(self.rtvDsvIndex);
      if (self.usage.has(kt::rhi::ImageUsage::RenderTarget)) {
        RHI::get().dxUpdateRenderTargetImage(i.value());
      } else if (self.usage.has(kt::rhi::ImageUsage::DepthStencil)) {
        RHI::get().dxUpdateDepthStencilImage(i.value());
      }
      if (self.usage.has(kt::rhi::ImageUsage::Sampled)) {
        RHI::get().dxUpdateSampledImage(i.value());
      }
    }
    return i;
  }

  void ImageBase::destroy(this Image& self) {
    if (self.allocation) {
#ifndef KT_DISABLE_STATS
      auto& stats = RHI::get().getStats();
      if (self._flags.has(ImageCreateFlags::EngineImage)) {
        --stats.engineImageCount;
        stats.engineImageMemUsage -= self.allocation->GetSize();
      } else {
        --stats.userImageCount;
        stats.userImageMemUsage -= self.allocation->GetSize();
      }
#endif

      self.allocation.reset();
    }
  }

  Image::Image(Image&& other) noexcept
      : ImageBase(std::move(other)), allocation(std::move(other.allocation)), rtvDsvIndex(other.rtvDsvIndex) {}

  Image& Image::operator=(Image&& other) noexcept {
    if (this != &other) {
      ImageBase::operator=(std::move(other));

      allocation = std::move(other.allocation);
      rtvDsvIndex = other.rtvDsvIndex;
    }
    return *this;
  }

  ImageBase::operator ImageRef(this const Image& self) {

    return ImageRef(self.name.c_str(), self.dxresource().Get(), self._dim, self._format, self.extent, self._mips, self._layers,
                    self.usage.has(kt::rhi::ImageUsage::RenderTarget) ? RHI::get().dxGetRtvHandle(self.rtvDsvIndex)
                    : self.usage.has(ImageUsage::DepthStencil)        ? RHI::get().dxGetDsvHandle(self.rtvDsvIndex)
                                                                      : CD3DX12_CPU_DESCRIPTOR_HANDLE(),
                    self.textureIndex);
  }

  Allocation Image::dxTakeAllocation() { return std::move(allocation); }

  Image::~Image() { destroy(); }
} // namespace kt::rhi