#pragma once

#include <stb/image.h>

namespace keptech::core {
  class Image {
    Image(unsigned char* data, int width, int height, int channels,
          bool hdr = false) // NOLINT
        : data(data), width(width), height(height), channels(channels),
          isHDR(hdr) {}

  public:
    Image() = default;

    Image(const Image&) = delete;
    Image(Image&& o) noexcept
        : data(o.data), width(o.width), height(o.height), channels(o.channels) {
      o.data = nullptr;
      o.width = 0;
      o.height = 0;
      o.channels = 0;
    }
    Image& operator=(const Image&) = delete;
    Image& operator=(Image&& o) noexcept {
      if (this != &o) {
        data = o.data;
        width = o.width;
        height = o.height;
        channels = o.channels;

        o.data = nullptr;
        o.width = 0;
        o.height = 0;
        o.channels = 0;
      }
      return *this;
    }

    [[nodiscard]] const unsigned char* getData() const { return data; }
    [[nodiscard]] const float* getHdrData() const {
      return reinterpret_cast<const float*>(data);
    }
    [[nodiscard]] int getWidth() const { return width; }
    [[nodiscard]] int getHeight() const { return height; }
    [[nodiscard]] int getChannels() const { return channels; }
    [[nodiscard]] bool isHdr() const { return isHDR; }
    [[nodiscard]] size_t pixelComponentCount() const {
      return static_cast<size_t>(width) * static_cast<size_t>(height) *
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
      bool supported = false;
    };

    Info static queryFile(const char* path) {
      Info info;
      info.supported =
          stbi_info(path, &info.width, &info.height, &info.channels);
      return info;
    }

    std::expected<Image, std::string> static loadFromFile(
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
      return Image(data, width, height, channels);
    }

    std::expected<Image, std::string> static loadFromMemory(
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
      return Image(data, width, height, channels);
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
      return Image(reinterpret_cast<unsigned char*>(data), width, height,
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
      return Image(reinterpret_cast<unsigned char*>(data), width, height,
                   channels, true);
    }

    ~Image() {
      if (data) {
        stbi_image_free(data);
      }
      data = nullptr;
    }

  private:
    unsigned char* data = nullptr;
    int width = 0;
    int height = 0;
    int channels = 0;
    bool isHDR = false;
  };
} // namespace keptech::core
