#include "image.hpp"
#include "helpers/transitions.hpp"
#include "imageCreateInfo.hpp"
#include "keptech/core/result.hpp"
#include "renderer.hpp"
#include "vk-logger.hpp"
#include "wrappers/device.hpp"
#include <glm/vec3.hpp>

namespace kt::rdr {
  kt::Result<Image, VkResult, VK_SUCCESS> Image::create(const ImageCreateInfo& info) {
    auto& r = Renderer::get();

    VkImage image = VK_NULL_HANDLE;
    VmaAllocation alloc{};
    VmaAllocationInfo aInfo = {};
    auto res = vmaCreateImage(r.getDevice(), &info.getImageInfo(), &info.getAllocInfo(), &image, &alloc, &aInfo);
    if (res != VK_SUCCESS) {
      return {res};
    }

    VkImageViewCreateInfo viewInfo = info.getViewInfo();
    viewInfo.image = image;

    VkImageView view = VK_NULL_HANDLE;
    res = vkCreateImageView(r.getDevice(), &viewInfo, nullptr, &view);
    if (res != VK_SUCCESS) {
      return {res};
    }

    VK_TRACE("Created image [{}] with size {} bytes, format {}, extent: {}x{}x{}", info.getName(), aInfo.size, info.getImageInfo().format,
             info.getImageInfo().extent.width, info.getImageInfo().extent.height, info.getImageInfo().extent.depth);

    ImageType imgType = ImageType::Color;

    switch (info.getImageInfo().format) {
    case VK_FORMAT_D16_UNORM:
    case VK_FORMAT_D32_SFLOAT:
      imgType = ImageType::Depth;
      break;
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
    case VK_FORMAT_D24_UNORM_S8_UINT:
      imgType = ImageType::DepthStencil;
      break;
    case VK_FORMAT_S8_UINT:
      imgType = ImageType::Stencil;
      break;
    default:
      break;
    }

    Image i(info.getName(), imgType, image, view, alloc, info.getImageInfo().extent, info.getImageInfo().format, info.getImageInfo().usage);
    if (info.getName()) {
      r->setAllocationName(i.alloc, info.getName());
      r->setDebugName(i, info.getName());
    }

    return std::move(i);
  }

  void Image::destroy() {
    if (image) {
      vmaDestroyImage(Renderer::get().getDevice(), image, alloc);
      vkDestroyImageView(Renderer::get().getDevice(), view, nullptr);
      image = nullptr;
      view = nullptr;
      alloc = nullptr;
    }
  }
  Image::operator VkImage() const { return image; }

  Image::operator VkImageView() const { return view; }
  [[nodiscard]] const glm::uvec3 Image::extent() const { return glm::uvec3{_extent.width, _extent.height, _extent.depth}; }
  [[nodiscard]] VkFormat Image::format() const { return _format; }

  ImageType Image::type() const { return _type; }
  uint8_t Image::mips() const { return _mips; }
  uint8_t Image::layers() const { return _layers; }
  VkImageLayout Image::getLayout() const { return currentLayout; }
  VkImageUsageFlags Image::getUsage() const { return usage; }

  [[nodiscard]] VkImageMemoryBarrier2 Image::transition(const TransitionInfo& transition, uint32_t srcIndex, uint32_t dstIndex) const {
    auto b = transition.image(image);
    b.srcQueueFamilyIndex = srcIndex;
    b.dstQueueFamilyIndex = dstIndex;
    return b;
  }
  [[nodiscard]] bool Image::hasHandle() const { return _handle != INVALID_HANDLE; }

  [[nodiscard]] ImageHandle Image::handle() const { return _handle; }
  void Image::setHandle(ImageHandle handle) { _handle = handle; }
  VkImageSubresourceRange Image::getSubresourceRange() const {
    VkImageAspectFlags aspectMask = 0;
    switch (_format) {
    case VK_FORMAT_D16_UNORM:
    case VK_FORMAT_X8_D24_UNORM_PACK32:
    case VK_FORMAT_D32_SFLOAT:
      aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
      break;
    case VK_FORMAT_S8_UINT:
      aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
      break;
    case VK_FORMAT_D16_UNORM_S8_UINT:
    case VK_FORMAT_D24_UNORM_S8_UINT:
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
      aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
      break;
    default:
      aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      break;
    }
    return VkImageSubresourceRange{
        .aspectMask = aspectMask,
        .baseMipLevel = 0,
        .levelCount = _mips,
        .baseArrayLayer = 0,
        .layerCount = _layers,
    };
  }
  Image::Image(Image&& other) noexcept
      : _type(other._type), _mips(other._mips), _layers(other._layers), image(other.image), view(other.view), alloc(other.alloc),
        _extent(other._extent), _format(other._format), _handle(other._handle) {
    other.image = VK_NULL_HANDLE;
    other.view = VK_NULL_HANDLE;
    other.alloc = nullptr;
    other._handle = INVALID_HANDLE;
  }
  Image& Image::operator=(Image&& other) noexcept {
    if (this != &other) {
      _type = other._type;
      _mips = other._mips;
      _layers = other._layers;
      image = other.image;
      view = other.view;
      alloc = other.alloc;
      _extent = other._extent;
      _format = other._format;
      _handle = other._handle;

      other.image = VK_NULL_HANDLE;
      other.view = VK_NULL_HANDLE;
      other.alloc = nullptr;
      other._handle = INVALID_HANDLE;
    }
    return *this;
  }
  Image::~Image() { destroy(); }
  VmaAllocation Image::getAllocation() const { return alloc; }

  Image::Image(std::string name, ImageType type, VkImage image, VkImageView view, VmaAllocation alloc, VkExtent3D extent, VkFormat format,
               VkImageUsageFlags usage)
      : name(std::move(name)), _type(type), image(image), view(view), alloc(alloc), _extent(extent), _format(format), usage(usage) {}

} // namespace kt::rdr