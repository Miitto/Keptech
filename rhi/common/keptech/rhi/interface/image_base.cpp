#include "image_base.hpp"

namespace kt::rhi {
  ImageFormat ImageBase::format() const { return _format; }
  ImageDim ImageBase::dim() const { return _dim; }
  uint32_t ImageBase::mips() const { return _mips; }
  uint32_t ImageBase::layers() const { return _layers; }
  glm::uvec3 ImageBase::getExtent() const { return extent; }
  const std::string& ImageBase::getName() const { return name; }
  Bitflag<ImageUsage> ImageBase::getUsage() const { return usage; }
  void ImageBase::setTextureIndex(uint64_t index) { textureIndex = index; }
  uint64_t ImageBase::getTextureIndex() const { return textureIndex; }
} // namespace kt::rhi