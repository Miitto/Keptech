#pragma once

#include "keptech/imageFile.hpp"

namespace kt {
  class StbImageFile : public ImageFile {
  public:
    static Result<StbImageFile, ImageFileLoadError, ImageFileLoadError::None> fromFile(std::string name, const std::string& path,
                                                                                       int desiredChannels = 0);

    uint32_t getLayerCount() const override;
    void* getLayerData(uint32_t layer) const override;
    size_t getLayerSize() const override;
    size_t getTotalByteSize() const override;

    uint32_t getWidth() const override;
    uint32_t getHeight() const override;
    uint32_t getDepth() const override;
    glm::uvec3 getExtent() const override;
    rhi::ImageFormat getFormat() const override;
    rhi::ImageDim getDim() const override;

    StbImageFile(std::string name, std::string path, void* data, uint32_t width, uint32_t height, uint32_t channels, bool isHdr)
        : ImageFile(std::move(name)), path(std::move(path)), data(data), width(width), height(height), channels(channels), isHdr(isHdr) {}
    StbImageFile(const StbImageFile&) = delete;
    StbImageFile& operator=(const StbImageFile&) = delete;
    StbImageFile(StbImageFile&&) noexcept;
    StbImageFile& operator=(StbImageFile&&) noexcept;
    ~StbImageFile() override;

  private:
    std::string path;
    void* data;
    uint32_t width;
    uint32_t height;
    uint32_t channels;
    bool isHdr;
  };
} // namespace kt