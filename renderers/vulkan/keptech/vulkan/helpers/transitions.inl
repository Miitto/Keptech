constexpr VkImageAspectFlags toVkImageAspectFlags(kt::ImageType t) {
  switch (t) {
  case kt::ImageType::Color:
    return VK_IMAGE_ASPECT_COLOR_BIT;
  case kt::ImageType::Depth:
    return VK_IMAGE_ASPECT_DEPTH_BIT;
  case kt::ImageType::Stencil:
    return VK_IMAGE_ASPECT_STENCIL_BIT;
  case kt::ImageType::DepthStencil:
    return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
  default:
    return 0;
  }
}

constexpr VkImageLayout toVkImageLayout(kt::ImageType t, kt::ImageLayout layout) {
  switch (layout) {
  case kt::ImageLayout::Undefined:
    return VK_IMAGE_LAYOUT_UNDEFINED;
  case kt::ImageLayout::RenderTarget: {
    switch (t) {
    case kt::ImageType::Color:
      return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    case kt::ImageType::Depth:
    case kt::ImageType::Stencil:
    case kt::ImageType::DepthStencil:
      return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    }
  }
  case kt::ImageLayout::ShaderReadOnly: {
    switch (t) {
    case kt::ImageType::Color:
      return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    case kt::ImageType::Depth:
    case kt::ImageType::Stencil:
    case kt::ImageType::DepthStencil:
      return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    }
  }
  case kt::ImageLayout::TransferSrc:
    return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  case kt::ImageLayout::TransferDst:
    return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  case kt::ImageLayout::ComputeReadWrite:
    return VK_IMAGE_LAYOUT_GENERAL;
  case kt::ImageLayout::Present:
    return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  default:
    return VK_IMAGE_LAYOUT_UNDEFINED;
  }
}

constexpr VkPipelineStageFlags2 toVkPipelineStageFlags(kt::ImageType imageType, kt::ImageLayout layout) {
  switch (layout) {
  case kt::ImageLayout::Undefined:
    return VK_PIPELINE_STAGE_2_NONE;
  case kt::ImageLayout::RenderTarget: {
    switch (imageType) {
    case kt::ImageType::Color:
      return VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    case kt::ImageType::Depth:
    case kt::ImageType::Stencil:
    case kt::ImageType::DepthStencil:
      return VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
    }
  }
  case kt::ImageLayout::ShaderReadOnly:
    return VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
  case kt::ImageLayout::TransferSrc:
  case kt::ImageLayout::TransferDst:
    return VK_PIPELINE_STAGE_2_TRANSFER_BIT;
  case kt::ImageLayout::ComputeReadWrite:
    return VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
  case kt::ImageLayout::Present:
    return VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
  default:
    return VK_PIPELINE_STAGE_2_NONE;
  }
}

constexpr VkAccessFlags2 toVkAccessFlags(kt::ImageType imageType, kt::ImageLayout layout) {
  switch (layout) {
  case kt::ImageLayout::Undefined:
    return 0;
  case kt::ImageLayout::RenderTarget: {
    switch (imageType) {
    case kt::ImageType::Color:
      return VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT;
    case kt::ImageType::Depth:
    case kt::ImageType::Stencil:
    case kt::ImageType::DepthStencil:
      return VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
    }
  }
  case kt::ImageLayout::ShaderReadOnly:
    return VK_ACCESS_2_SHADER_READ_BIT;
  case kt::ImageLayout::TransferSrc:
    return VK_ACCESS_2_TRANSFER_READ_BIT;
  case kt::ImageLayout::TransferDst:
    return VK_ACCESS_2_TRANSFER_WRITE_BIT;
  case kt::ImageLayout::ComputeReadWrite:
    return VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
  default:
    return 0;
  }
}