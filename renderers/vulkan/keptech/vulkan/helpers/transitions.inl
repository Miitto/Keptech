
consteval VkImageAspectFlags toVkImageAspectFlags(ImageType t) {
  switch (t) {
  case ImageType::Color:
    return VK_IMAGE_ASPECT_COLOR_BIT;
  case ImageType::DepthStencil:
    return VK_IMAGE_ASPECT_DEPTH_BIT;
  default:
    return 0;
  }
}

consteval VkImageLayout toVkImageLayout(ImageType t, ImageLayout layout) {
  switch (layout) {
  case ImageLayout::Undefined:
    return VK_IMAGE_LAYOUT_UNDEFINED;
  case ImageLayout::RenderTarget: {
    switch (t) {
    case ImageType::Color:
      return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    case ImageType::DepthStencil:
      return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    }
  }
  case ImageLayout::ShaderReadOnly: {
    switch (t) {
    case ImageType::Color:
      return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    case ImageType::DepthStencil:
      return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    }
  }
  case ImageLayout::TransferSrc:
    return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  case ImageLayout::TransferDst:
    return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  case ImageLayout::ComputeReadWrite:
    return VK_IMAGE_LAYOUT_GENERAL;
  case ImageLayout::Present:
    return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  default:
    return VK_IMAGE_LAYOUT_UNDEFINED;
  }
}

consteval VkPipelineStageFlags2 toVkPipelineStageFlags(ImageType imageType, ImageLayout layout) {
  switch (layout) {
  case ImageLayout::Undefined:
    return VK_PIPELINE_STAGE_2_NONE;
  case ImageLayout::RenderTarget: {
    switch (imageType) {
    case ImageType::Color:
      return VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    case ImageType::DepthStencil:
      return VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
    }
  }
  case ImageLayout::ShaderReadOnly:
    return VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
  case ImageLayout::TransferSrc:
  case ImageLayout::TransferDst:
    return VK_PIPELINE_STAGE_2_TRANSFER_BIT;
  case ImageLayout::ComputeReadWrite:
    return VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
  case ImageLayout::Present:
    return VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
  default:
    return VK_PIPELINE_STAGE_2_NONE;
  }
}

consteval VkAccessFlags2 toVkAccessFlags(ImageType imageType, ImageLayout layout) {
  switch (layout) {
  case ImageLayout::Undefined:
    return 0;
  case ImageLayout::RenderTarget: {
    switch (imageType) {
    case ImageType::Color:
      return VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT;
    case ImageType::DepthStencil:
      return VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
    }
  }
  case ImageLayout::ShaderReadOnly:
    return VK_ACCESS_2_SHADER_READ_BIT;
  case ImageLayout::TransferSrc:
    return VK_ACCESS_2_TRANSFER_READ_BIT;
  case ImageLayout::TransferDst:
    return VK_ACCESS_2_TRANSFER_WRITE_BIT;
  case ImageLayout::ComputeReadWrite:
    return VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
  default:
    return 0;
  }
}