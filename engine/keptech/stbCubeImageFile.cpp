#include "keptech/stbCubeImageFile.hpp"

#include <filesystem>
#include <stb/image.h>

namespace kt {
  Result<StbCubeImageFile, ImageFileLoadError, ImageFileLoadError::None>
  StbCubeImageFile::fromFile(std::string name, const std::string& posX, const std::string& negX, const std::string& posY,
                             const std::string& negY, const std::string& posZ, const std::string& negZ, int desiredChannels) {
    if (!std::filesystem::exists(posX)) {
      KT_ERROR("+X file does not exist: {}", posX);
      return ImageFileLoadError::FileNotFound;
    }
    if (!std::filesystem::exists(negX)) {
      KT_ERROR("-X file does not exist: {}", negX);
      return ImageFileLoadError::FileNotFound;
    }
    if (!std::filesystem::exists(posY)) {
      KT_ERROR("+Y file does not exist: {}", posY);
      return ImageFileLoadError::FileNotFound;
    }
    if (!std::filesystem::exists(negY)) {
      KT_ERROR("-Y file does not exist: {}", negY);
      return ImageFileLoadError::FileNotFound;
    }
    if (!std::filesystem::exists(posZ)) {
      KT_ERROR("+Z file does not exist: {}", posZ);
      return ImageFileLoadError::FileNotFound;
    }
    if (!std::filesystem::exists(negZ)) {
      KT_ERROR("-Z file does not exist: {}", negZ);
      return ImageFileLoadError::FileNotFound;
    }

    std::array<FaceData, 6> faces = {{
        {posX, nullptr},
        {negX, nullptr},
        {posY, nullptr},
        {negY, nullptr},
        {negZ, nullptr},
        {posZ, nullptr},
    }};

    int width{}, height{}, channels{};
    bool isHdr = stbi_is_hdr(posX.c_str());

#ifndef NDEBUG
    stbi_info(posX.c_str(), &width, &height, &channels);
    bool consistent = true;
    for (size_t i = 1; i < faces.size(); ++i) {
      auto& face = faces[i];
      int w{}, h{}, c{};
      bool hdr = stbi_is_hdr(face.path.c_str());
      if (hdr != isHdr) {
        KT_ERROR("Inconsistent HDR state between cube map faces: {} is {}, {} is {}", posX, isHdr ? "HDR" : "LDR", face.path,
                 hdr ? "HDR" : "LDR");
        consistent = false;
      }
      stbi_info(face.path.c_str(), &w, &h, &c);
      if (w != width || h != height || c != channels) {
        KT_ERROR("Inconsistent dimensions between cube map faces: {} is {}x{}x{}b, {} is {}x{}x{}b", posX, width, height, channels,
                 face.path, w, h, c);
        consistent = false;
      }
    }

    if (!consistent) {
      return ImageFileLoadError::InvalidFormat;
    }
#endif

    bool errored = false;
    if (isHdr) {
      for (auto& face : faces) {
        face.data = stbi_loadf(face.path.c_str(), &width, &height, &channels, desiredChannels);
        if (!face.data) {
          const char* errorMessage = stbi_failure_reason();
          KT_ERROR("Failed to load HDR image '{}': {}", face.path, errorMessage);
          errored = true;
          break;
        }
      }
    } else {
      for (auto& face : faces) {
        face.data = stbi_load(face.path.c_str(), &width, &height, &channels, desiredChannels);

        if (!face.data) {
          const char* errorMessage = stbi_failure_reason();
          KT_ERROR("Failed to load LDR image '{}': {}", face.path, errorMessage);
          errored = true;
          break;
        }
      }
    }

    if (errored) {
      for (auto& face : faces) {
        if (face.data) {
          stbi_image_free(face.data);
        }
      }
      return ImageFileLoadError::LoadError;
    }

    return StbCubeImageFile(std::move(name), faces, static_cast<uint32_t>(width), static_cast<uint32_t>(height),
                            desiredChannels == 0 ? static_cast<uint32_t>(channels) : static_cast<uint32_t>(desiredChannels), isHdr);
  }

  StbCubeImageFile::StbCubeImageFile(StbCubeImageFile&& other) noexcept
      : ImageFile(std::move(other)), faces(std::move(other.faces)), width(other.width), height(other.height), channels(other.channels),
        isHdr(other.isHdr) {
    for (auto& face : other.faces) {
      face.data = nullptr;
    }
  }
  StbCubeImageFile& StbCubeImageFile::operator=(StbCubeImageFile&& other) noexcept {
    if (this != &other) {
      ImageFile::operator=(std::move(other));
      faces = std::move(other.faces);
      width = other.width;
      height = other.height;
      channels = other.channels;
      isHdr = other.isHdr;

      for (auto& face : other.faces) {
        face.data = nullptr;
      }
    }
    return *this;
  }

  StbCubeImageFile::~StbCubeImageFile() {
    for (auto& face : faces) {
      if (face.data) {
        stbi_image_free(face.data);
      }
    }
  }

  uint32_t StbCubeImageFile::getLayerCount() const { return 6; }
  void* StbCubeImageFile::getLayerData(uint32_t layer) const {
    KT_ASSERT(layer < 6, "Invalid layer");
    return faces[layer].data;
  }
  size_t StbCubeImageFile::getLayerSize() const {
    return static_cast<size_t>(width) * static_cast<size_t>(height) * static_cast<size_t>(channels) * ((isHdr * 3) + 1);
  }
  size_t StbCubeImageFile::getTotalByteSize() const {
    return getLayerSize() * 6; // 6 faces
  }

  uint32_t StbCubeImageFile::getWidth() const { return width; }
  uint32_t StbCubeImageFile::getHeight() const { return height; }
  uint32_t StbCubeImageFile::getDepth() const { return 1; }
  glm::uvec3 StbCubeImageFile::getExtent() const { return {width, height, 1}; }
  rhi::ImageDim StbCubeImageFile::getDim() const { return rhi::ImageDim::eCube; }
  rhi::ImageFormat StbCubeImageFile::getFormat() const {
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