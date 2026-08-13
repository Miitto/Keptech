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
  public:
    ImageFormat format() const;
    ImageDim dim() const;
    uint32_t mips() const;
    uint32_t layers() const;
    glm::uvec3 getExtent() const;
    const std::string& getName() const;
    Bitflag<ImageUsage> getUsage() const;

    bool isDepth() const;

    void destroy();

    Microsoft::WRL::ComPtr<ID3D12Resource> dxresource() const;
    uint16_t dxGetRtvDsvIndex() const { return rtvDsvIndex; }
    void dxSetRtvDsvIndex(uint16_t index) { rtvDsvIndex = index; }

    operator ImageRef() const;

    static Result<Image, HRESULT, S_OK> create(const ImageCreateInfo& info);

    Image() = default;
    Image(const Image&) = delete;
    Image& operator=(const Image&) = delete;
    Image(Image&& other) noexcept;
    Image& operator=(Image&& other) noexcept;
    ~Image() { destroy(); }

    kt::Result<Image, HRESULT, S_OK> resize(const glm::uvec3& newExtent) const;

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
  };
} // namespace kt::rhi