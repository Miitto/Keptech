#pragma once

#include <expected>
#include <glm/glm.hpp>
#include <stb/image.h>
#include <string>

namespace kt {
  class Image {
    Image(unsigned char* data, glm::ivec2 size, int channels, bool hdr = false)
        : data(data), size(size), channels(channels), isHDR(hdr) {}

  public:
    Image() = default;

    Image(const Image&) = delete;
    Image(Image&& o) noexcept
        : data(o.data), size(o.size), channels(o.channels) {
      o.size = {0, 0};
      o.data = nullptr;
      o.channels = 0;
    }
    Image& operator=(const Image&) = delete;
    Image& operator=(Image&& o) noexcept {
      if (this != &o) {
        data = o.data;
        channels = o.channels;
        size = o.size;

        o.data = nullptr;
        o.size = {0, 0};
        o.channels = 0;
      }
      return *this;
    }

    [[nodiscard]] const unsigned char* getData() const { return data; }
    [[nodiscard]] const float* getHdrData() const {
      return reinterpret_cast<const float*>(data);
    }
    [[nodiscard]] glm::ivec2 getSize() const { return size; }
    [[nodiscard]] int getWidth() const { return size.x; }
    [[nodiscard]] int getHeight() const { return size.y; }
    [[nodiscard]] int getChannels() const { return channels; }
    [[nodiscard]] bool isHdr() const { return isHDR; }
    [[nodiscard]] size_t pixelComponentCount() const {
      return static_cast<size_t>(size.x) * static_cast<size_t>(size.y) *
             static_cast<size_t>(channels);
    }
    [[nodiscard]] size_t getByteSize() const {
      return pixelComponentCount() *
             (isHDR ? sizeof(float) : sizeof(unsigned char));
    }

    struct Info {
      int width = 0;
      int height = 0;
      int channels = 0;
      bool hrd = false;
      bool supported = false;
    };

    static void setFlipOnLoad(bool flip) {
      stbi_set_flip_vertically_on_load(flip ? 1 : 0);
    }

    bool static isFileHdr(const char* path) { return stbi_is_hdr(path) != 0; }

    Info static queryFile(const char* path) {
      Info info;
      info.supported =
          stbi_info(path, &info.width, &info.height, &info.channels) != 0;
      info.hrd = isFileHdr(path);
      return info;
    }

    std::expected<Info, std::string> static queryMemory(
        const unsigned char* buffer, size_t size) {
      Info info;
      info.supported =
          stbi_info_from_memory(buffer, static_cast<int>(size), &info.width,
                                &info.height, &info.channels) != 0;
      if (!info.supported) {
        return std::unexpected(
            std::string("Failed to query image info from memory buffer"));
      }
      info.hrd = stbi_is_hdr_from_memory(buffer, static_cast<int>(size)) != 0;
      return info;
    }

    size_t static getMemoryRequiredSizeLDR(int width, int height,
                                           int channels) {
      return static_cast<size_t>(width) * static_cast<size_t>(height) *
             static_cast<size_t>(channels) * sizeof(unsigned char);
    }

    size_t static getMemoryRequiredSizeHDR(int width, int height,
                                           int channels) {
      return static_cast<size_t>(width) * static_cast<size_t>(height) *
             static_cast<size_t>(channels) * sizeof(float);
    }

    size_t static getMemoryRequiredSize(int width, int height, int channels,
                                        bool hdr) {
      if (hdr) {
        return getMemoryRequiredSizeHDR(width, height, channels);
      } else {
        return getMemoryRequiredSizeLDR(width, height, channels);
      }
    }

    std::expected<Image, std::string> static loadFromFileLDR(
        const char* path, int desiredChannels = 0) {
      int width = 0;
      int height = 0;
      int channels = 0;
      unsigned char* data =
          stbi_load(path, &width, &height, &channels, desiredChannels);
      if (!data) {
        return std::unexpected(std::string("Failed to load image from file: ") +
                               path);
      }
      if (desiredChannels != 0) {
        channels = desiredChannels;
      }
      return Image(data, {width, height}, channels);
    }

    std::expected<Image, std::string> static loadFromMemoryLDR(
        const unsigned char* buffer, size_t size, int desiredChannels = 0) {
      int width = 0;
      int height = 0;
      int channels = 0;
      unsigned char* data =
          stbi_load_from_memory(buffer, static_cast<int>(size), &width, &height,
                                &channels, desiredChannels);
      if (!data) {
        return std::unexpected(
            std::string("Failed to load image from memory buffer"));
      }
      if (desiredChannels != 0) {
        channels = desiredChannels;
      }
      return Image(data, {width, height}, channels);
    }

    std::expected<Image, std::string> static loadFromFileHDR(
        const char* path, int desiredChannels = 0) {
      int width = 0;
      int height = 0;
      int channels = 0;
      float* data =
          stbi_loadf(path, &width, &height, &channels, desiredChannels);
      if (!data) {
        return std::unexpected(
            std::string("Failed to load HDR image from file: ") + path);
      }
      if (desiredChannels != 0) {
        channels = desiredChannels;
      }
      return Image(reinterpret_cast<unsigned char*>(data), {width, height},
                   channels, true);
    }

    std::expected<Image, std::string> static loadFromMemoryHDR(
        const float* buffer, size_t size, int desiredChannels = 0) {
      int width = 0;
      int height = 0;
      int channels = 0;
      float* data = stbi_loadf_from_memory(
          reinterpret_cast<const unsigned char*>(buffer),
          static_cast<int>(size), &width, &height, &channels, desiredChannels);
      if (!data) {
        return std::unexpected(
            std::string("Failed to load HDR image from memory buffer"));
      }
      if (desiredChannels != 0) {
        channels = desiredChannels;
      }
      return Image(reinterpret_cast<unsigned char*>(data), {width, height},
                   channels, true);
    }

    std::expected<Image, std::string> static loadFromFile(
        const char* path, int desiredChannels = 0) {
      if (isFileHdr(path)) {
        return loadFromFileHDR(path, desiredChannels);
      } else {
        return loadFromFileLDR(path, desiredChannels);
      }
    }

    std::expected<Image, std::string> static loadFromMemory(
        const unsigned char* buffer, size_t size, int desiredChannels = 0) {
      auto infoOrError = queryMemory(buffer, size);
      if (!infoOrError.has_value()) {
        return std::unexpected(infoOrError.error());
      }
      if (infoOrError->hrd) {
        return loadFromMemoryHDR(reinterpret_cast<const float*>(buffer), size,
                                 desiredChannels);
      } else {
        return loadFromMemoryLDR(buffer, size, desiredChannels);
      }
    }

    ~Image() {
      if (data) {
        stbi_image_free(data);
      }
      data = nullptr;
    }

  private:
    unsigned char* data = nullptr;
    glm::ivec2 size{0, 0};
    int channels = 0;
    bool isHDR = false;
  };
} // namespace kt
