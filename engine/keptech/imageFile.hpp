#pragma once

#include "keptech/core/result.hpp"
#include "keptech/rhi/imageCreateInfo.hpp"
#include "keptech/rhi/imageFormat.hpp"
#include <spdlog/fmt/bundled/format.h>
#include <string>
#include <string_view>

namespace kt {

  enum class ImageFileLoadError : uint8_t {
    None,
    FileNotFound,
    InvalidFormat,
    LoadError,
  };

  class ImageFile {
  public:
    static Result<std::unique_ptr<ImageFile>, ImageFileLoadError, ImageFileLoadError::None> fromFile(std::string name,
                                                                                                     const std::string& path);
    static Result<std::unique_ptr<ImageFile>, ImageFileLoadError, ImageFileLoadError::None>
    cubeFromFile(std::string name, const std::string_view& posX, const std::string_view& negX, const std::string_view& posY,
                 const std::string_view& negY, const std::string_view& posZ, const std::string_view& negZ);

    [[nodiscard]] const std::string& getName() const { return name; }
    ImageFile& setName(std::string newName) {
      name = std::move(newName);
      return *this;
    }

    virtual uint32_t getLayerCount() const = 0;
    virtual void* getLayerData(uint32_t layer) const = 0;
    virtual size_t getLayerSize() const = 0;
    virtual size_t getTotalByteSize() const = 0;

    virtual uint32_t getWidth() const = 0;
    virtual uint32_t getHeight() const = 0;
    virtual uint32_t getDepth() const = 0;
    virtual glm::uvec3 getExtent() const = 0;
    virtual rhi::ImageFormat getFormat() const = 0;
    virtual rhi::ImageDim getDim() const = 0;

    ImageFile() = default;
    ImageFile(std::string name) : name(std::move(name)) {}
    ImageFile(const ImageFile&) = default;
    ImageFile(ImageFile&&) noexcept = default;
    ImageFile& operator=(const ImageFile&) = default;
    ImageFile& operator=(ImageFile&&) noexcept = default;
    virtual ~ImageFile() = default;

  private:
    std::string name;
  };
} // namespace kt

template <> struct fmt::formatter<kt::ImageFileLoadError> : fmt::formatter<std::string_view> {
  fmt::format_context::iterator format(const kt::ImageFileLoadError& error, fmt::format_context& ctx) const;
};