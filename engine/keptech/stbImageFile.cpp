#include "keptech/stbImageFile.hpp"

#include <filesystem>
#include <stb/image.h>

namespace kt {
  Result<StbImageFile, ImageFileLoadError, ImageFileLoadError::None> StbImageFile::fromFile(std::string name, const std::string& path,
                                                                                            int desiredChannels) {
    if (!std::filesystem::exists(path)) {
      return ImageFileLoadError::FileNotFound;
    }

    int width{}, height{}, channels{};
    bool isHdr = stbi_is_hdr(path.c_str());
    void* data = nullptr;
    if (isHdr) {
      data = stbi_loadf(path.c_str(), &width, &height, &channels, desiredChannels);
    } else {
      data = stbi_load(path.c_str(), &width, &height, &channels, desiredChannels);
    }

    if (!data) {
      const char* errorMessage = stbi_failure_reason();
      KT_ERROR("Failed to load image '{}': {}", path, errorMessage);
      return ImageFileLoadError::LoadError;
    }

    return StbImageFile(std::move(name), path, data, static_cast<uint32_t>(width), static_cast<uint32_t>(height),
                        desiredChannels == 0 ? static_cast<uint32_t>(channels) : static_cast<uint32_t>(desiredChannels), isHdr);
  }

  StbImageFile::StbImageFile(StbImageFile&& other) noexcept
      : ImageFile(std::move(other)), path(std::move(other.path)), data(other.data), width(other.width), height(other.height),
        channels(other.channels), isHdr(other.isHdr) {
    other.data = nullptr;
  }
  StbImageFile& StbImageFile::operator=(StbImageFile&& other) noexcept {
    if (this != &other) {
      ImageFile::operator=(std::move(other));
      path = std::move(other.path);
      data = other.data;
      width = other.width;
      height = other.height;
      channels = other.channels;
      isHdr = other.isHdr;

      other.data = nullptr;
    }
    return *this;
  }

  StbImageFile::~StbImageFile() {
    if (data) {
      stbi_image_free(data);
    }
  }

  uint32_t StbImageFile::getLayerCount() const { return 1; }
  void* StbImageFile::getLayerData(uint32_t layer) const {
    KT_ASSERT(layer == 0, "Invalid layer index {} for image with only 1 layer", layer);
    return data;
  }
  size_t StbImageFile::getLayerSize() const {
    //                                                                                                Branchless x4 if HDR
    return static_cast<size_t>(width) * static_cast<size_t>(height) * static_cast<size_t>(channels) * ((isHdr * 3) + 1);
  }
  size_t StbImageFile::getTotalByteSize() const { return getLayerSize(); }

  uint32_t StbImageFile::getWidth() const { return width; }
  uint32_t StbImageFile::getHeight() const { return height; }
  uint32_t StbImageFile::getDepth() const { return 1; }
  glm::uvec3 StbImageFile::getExtent() const { return {width, height, 1}; }
  rhi::ImageDim StbImageFile::getDim() const { return rhi::ImageDim::e2D; }
  rhi::ImageFormat StbImageFile::getFormat() const {
    if (isHdr) {
      switch (channels) {
      case 1:
        return rhi::ImageFormat::R32_FLOAT;
      case 2:
        return rhi::ImageFormat::R32G32_FLOAT;
      case 3:
        return rhi::ImageFormat::R32G32B32_FLOAT;
      case 4:
        return rhi::ImageFormat::R32G32B32A32_FLOAT;
      default:
        KT_ABORT("Invalid number of channels");
      }
    } else {
      switch (channels) {
      case 1:
        return rhi::ImageFormat::R8_UNORM;
      case 2:
        return rhi::ImageFormat::R8G8_UNORM;
      case 3:
        KT_WARN("3 channel 8 bit images are not be supported on all RHI backends.");
        return rhi::ImageFormat::R8G8B8_UNORM;
      case 4:
        return rhi::ImageFormat::R8G8B8A8_UNORM;
      default:
        KT_ABORT("Invalid number of channels");
      }
    }
  }
} // namespace kt