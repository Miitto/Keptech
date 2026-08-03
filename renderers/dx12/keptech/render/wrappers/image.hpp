#pragma once

#include "keptech/core/result.hpp"
#include "keptech/render/interface.hpp"

namespace D3D12MA {
  struct Allocation;
}

namespace kt::rdr {
  class ImageCreateInfo;

  class Image {
  public:
    ImageFormat format() const;
    uint32_t mips() const;
    uint32_t layers() const;
    const std::string& getName() const;

    void destroy();

    static Result<Image, HRESULT, S_OK> create(const ImageCreateInfo& info);

    Image() = default;
    Image(const Image&) = delete;
    Image& operator=(const Image&) = delete;
    Image(Image&& other) noexcept;
    Image& operator=(Image&& other) noexcept;
    ~Image() { destroy(); }

  private:
    Image(std::string name, ImageFormat format, uint32_t mips, uint32_t layers, D3D12MA::Allocation* allocation)
        : name(std::move(name)), _format(format), _mips(mips), _layers(layers), allocation(allocation) {}

    std::string name;
    ImageFormat _format;
    uint32_t _mips;
    uint32_t _layers;
    D3D12MA::Allocation* allocation = nullptr;
  };
} // namespace kt::rdr