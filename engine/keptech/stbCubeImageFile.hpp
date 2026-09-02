#pragma once

#include "keptech/imageFile.hpp"

namespace kt {
  class StbCubeImageFile : public ImageFile {
  public:
    static Result<StbCubeImageFile, ImageFileLoadError, ImageFileLoadError::None>
    fromFile(std::string name, const std::string& posX, const std::string& negX, const std::string& posY, const std::string& negY,
             const std::string& posZ, const std::string& negZ, int desiredChannels = 0);

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

    struct FaceData {
      std::string path;
      void* data;
    };

    StbCubeImageFile(std::string name, std::array<FaceData, 6> faces, uint32_t width, uint32_t height, uint32_t channels, bool isHdr)
        : ImageFile(std::move(name)), faces(std::move(faces)), width(width), height(height), channels(channels), isHdr(isHdr) {}
    StbCubeImageFile(const StbCubeImageFile&) = delete;
    StbCubeImageFile& operator=(const StbCubeImageFile&) = delete;
    StbCubeImageFile(StbCubeImageFile&&) noexcept;
    StbCubeImageFile& operator=(StbCubeImageFile&&) noexcept;
    ~StbCubeImageFile() override;

  private:
    std::array<FaceData, 6> faces;
    uint32_t width;
    uint32_t height;
    uint32_t channels;
    bool isHdr;
  };
} // namespace kt