
#include "keptech/core/result.hpp"
#include "keptech/rhi/imageCreateInfo.hpp"
#include "keptech/rhi/imageFormat.hpp"
#include "keptech/rhi/result.hpp"

namespace kt::rhi {
  class Image;
  class ImageRef;

  class ImageBase {
  public:
    friend class Image;

    ImageFormat format() const;
    ImageDim dim() const;
    uint32_t mips() const;
    uint32_t layers() const;
    glm::uvec3 getExtent() const;
    const std::string& getName() const;
    Bitflag<ImageUsage> getUsage() const;
    void setTextureIndex(uint64_t index);
    uint64_t getTextureIndex() const;

    void setName(this Image& self, const std::string& newName);

    bool operator==(this const Image& self, const Image& other);

    bool isDepth(this const Image& self);

    void destroy(this Image& self);

    operator ImageRef(this const Image& self);

    static Result<Image, RawRhiResult, RawRhiResultOk> create(const ImageCreateInfo& info);
    kt::Result<Image, RawRhiResult, RawRhiResultOk> resize(this const Image& self, const glm::uvec3& newExtent);

    ImageBase() = default;
    ImageBase(const ImageBase&) = delete;
    ImageBase& operator=(const ImageBase&) = delete;
    ImageBase(ImageBase&& other) = default;
    ImageBase& operator=(ImageBase&& other) = default;
    ~ImageBase() = default;

  protected:
    ImageBase(std::string name, ImageDim dim, ImageFormat format, glm::uvec3 extent, Bitflag<ImageUsage> usage, uint32_t mips,
              uint32_t layers)
        : name(std::move(name)), _dim(dim), _format(format), extent(extent), usage(usage), _mips(mips), _layers(layers) {}

  private:
    std::string name{};
    ImageDim _dim = ImageDim::e2D;
    ImageFormat _format = ImageFormat::Undefined;
    glm::uvec3 extent{};
    Bitflag<ImageUsage> usage{};
    uint32_t _mips = 1;
    uint32_t _layers = 1;
    uint64_t textureIndex = UINT64_MAX;
  };
} // namespace kt::rhi